#include "Freya/Asset/BoneMatrixResources.hpp"

#include "Freya/Builders/BufferBuilder.hpp"

#include <algorithm>
#include <cstring>

namespace FREYA_NAMESPACE
{
    BoneMatrixResources::BoneMatrixResources(const skr::Arc<Device>& device,
                                             const std::uint32_t     frameCount,
                                             const std::uint32_t     capacity) :
        mDevice(device), mFrameCount(std::max(1u, frameCount)),
        mCapacity(std::max(1u, capacity)), mCpuPrev(mCapacity, glm::mat4(1.f))
    {
        const auto totalSize = frameBytes() * mFrameCount;
        mBuffer = BufferBuilder(mDevice)
                      .SetUsage(BufferUsage::Storage)
                      .SetSize(static_cast<std::uint32_t>(totalSize))
                      .Build();

        {
            std::vector<glm::mat4> id(mCapacity * 2u * mFrameCount,
                                      glm::mat4(1.f));
            mBuffer->Copy(
                id.data(),
                static_cast<std::uint32_t>(id.size() * sizeof(glm::mat4)));
        }

        const auto stageFlags = vk::ShaderStageFlagBits::eVertex |
                                vk::ShaderStageFlagBits::eCompute;
        const auto bindings   = std::array {
            vk::DescriptorSetLayoutBinding()
                .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setStageFlags(stageFlags),
            vk::DescriptorSetLayoutBinding()
                .setBinding(1)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setStageFlags(stageFlags),
        };
        mLayout = mDevice->Get().createDescriptorSetLayout(
            vk::DescriptorSetLayoutCreateInfo().setBindings(bindings));

        const auto poolSizes = std::array {
            vk::DescriptorPoolSize()
                .setType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(mFrameCount * 2u),
        };
        mPool = mDevice->Get().createDescriptorPool(
            vk::DescriptorPoolCreateInfo()
                .setMaxSets(mFrameCount)
                .setPoolSizes(poolSizes));

        const auto layouts =
            std::vector<vk::DescriptorSetLayout>(mFrameCount, mLayout);
        mSets = mDevice->Get().allocateDescriptorSets(
            vk::DescriptorSetAllocateInfo()
                .setDescriptorPool(mPool)
                .setSetLayouts(layouts));

        const auto paletteBytes = PaletteBytes();
        for (std::uint32_t i = 0; i < mFrameCount; ++i)
        {
            const auto base      = BonesByteOffset(i);
            auto       bonesInfo = vk::DescriptorBufferInfo()
                                       .setBuffer(mBuffer->Get())
                                       .setOffset(base)
                                       .setRange(paletteBytes);
            auto       prevInfo  = vk::DescriptorBufferInfo()
                                       .setBuffer(mBuffer->Get())
                                       .setOffset(base + paletteBytes)
                                       .setRange(paletteBytes);
            const auto writes    = std::array {
                vk::WriteDescriptorSet()
                    .setDstSet(mSets[i])
                    .setDstBinding(0)
                    .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(bonesInfo),
                vk::WriteDescriptorSet()
                    .setDstSet(mSets[i])
                    .setDstBinding(1)
                    .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(prevInfo),
            };
            mDevice->Get().updateDescriptorSets(writes, {});
        }
    }

    BoneMatrixResources::~BoneMatrixResources()
    {
        if (!mDevice)
            return;
        if (mPool)
            mDevice->Get().destroyDescriptorPool(mPool);
        if (mLayout)
            mDevice->Get().destroyDescriptorSetLayout(mLayout);
    }

    void BoneMatrixResources::Upload(
        const std::uint32_t frameIndex, const std::span<const glm::mat4> bones)
    {
        const auto count =
            std::min(static_cast<std::uint32_t>(bones.size()), mCapacity);
        const auto fi   = frameIndex % mFrameCount;
        const auto base = static_cast<std::uint64_t>(BonesByteOffset(fi));
        const auto paletteBytes = static_cast<std::uint64_t>(PaletteBytes());

        std::vector<glm::mat4> cur(mCapacity, glm::mat4(1.f));
        if (count > 0)
            std::memcpy(cur.data(), bones.data(), count * sizeof(glm::mat4));

        // First pose: zero velocity so TAA does not keep a bind-pose ghost.
        if (!mHasUploaded)
        {
            mCpuPrev     = cur;
            mHasUploaded = true;
        }

        mBuffer->Copy(mCpuPrev.data(), paletteBytes, base + paletteBytes);
        mBuffer->Copy(cur.data(), paletteBytes, base);

        mCpuPrev = std::move(cur);
    }

    void BoneMatrixResources::RecordCarryBonesFromPreviousFrame(
        const vk::CommandBuffer commandBuffer,
        const std::uint32_t     frameIndex) const
    {
        if (mFrameCount < 2)
            return;

        const auto curFi  = frameIndex % mFrameCount;
        const auto prevFi = (curFi + mFrameCount - 1u) % mFrameCount;
        const auto srcOff = BonesByteOffset(prevFi);
        const auto dstOff = BonesByteOffset(curFi);
        const auto bytes  = PaletteBytes();

        // Cover both palettes so the following copy/barrier chain is ordered.
        const auto lo   = srcOff < dstOff ? srcOff : dstOff;
        const auto hi   = (srcOff < dstOff ? dstOff : srcOff) + bytes;
        const auto span = hi - lo;

        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eVertexShader |
                vk::PipelineStageFlagBits::eComputeShader |
                vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eTransfer, {}, {},
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eShaderRead |
                                  vk::AccessFlagBits::eShaderWrite |
                                  vk::AccessFlagBits::eTransferWrite)
                .setDstAccessMask(vk::AccessFlagBits::eTransferRead |
                                  vk::AccessFlagBits::eTransferWrite)
                .setBuffer(mBuffer->Get())
                .setOffset(lo)
                .setSize(span),
            {});

        const auto region =
            vk::BufferCopy().setSrcOffset(srcOff).setDstOffset(dstOff).setSize(
                bytes);
        commandBuffer.copyBuffer(mBuffer->Get(), mBuffer->Get(), region);

        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eTransfer |
                vk::PipelineStageFlagBits::eComputeShader,
            {}, {},
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
                .setDstAccessMask(vk::AccessFlagBits::eTransferRead |
                                  vk::AccessFlagBits::eTransferWrite |
                                  vk::AccessFlagBits::eShaderRead |
                                  vk::AccessFlagBits::eShaderWrite)
                .setBuffer(mBuffer->Get())
                .setOffset(dstOff)
                .setSize(bytes),
            {});
    }

    void BoneMatrixResources::RecordCopyCurrentToPrev(
        const vk::CommandBuffer commandBuffer,
        const std::uint32_t     frameIndex) const
    {
        const auto bonesOff = BonesByteOffset(frameIndex);
        const auto prevOff  = PrevBonesByteOffset(frameIndex);
        const auto bytes    = PaletteBytes();

        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eVertexShader |
                vk::PipelineStageFlagBits::eComputeShader |
                vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eTransfer, {}, {},
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eShaderRead |
                                  vk::AccessFlagBits::eShaderWrite |
                                  vk::AccessFlagBits::eTransferWrite)
                .setDstAccessMask(vk::AccessFlagBits::eTransferRead |
                                  vk::AccessFlagBits::eTransferWrite)
                .setBuffer(mBuffer->Get())
                .setOffset(bonesOff)
                .setSize(bytes + bytes),
            {});

        const auto region =
            vk::BufferCopy()
                .setSrcOffset(bonesOff)
                .setDstOffset(prevOff)
                .setSize(bytes);
        commandBuffer.copyBuffer(mBuffer->Get(), mBuffer->Get(), region);

        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer,
            vk::PipelineStageFlagBits::eComputeShader, {}, {},
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
                .setDstAccessMask(vk::AccessFlagBits::eShaderRead |
                                  vk::AccessFlagBits::eShaderWrite)
                .setBuffer(mBuffer->Get())
                .setOffset(bonesOff)
                .setSize(bytes + bytes),
            {});
    }

} // namespace FREYA_NAMESPACE
