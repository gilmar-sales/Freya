#include "Freya/Asset/BoneMatrixResources.hpp"

#include "Freya/Builders/BufferBuilder.hpp"

#include <algorithm>
#include <array>
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

        mDevice->Get().waitIdle();
        if (mPool)
            mDevice->Get().destroyDescriptorPool(mPool);
        if (mLayout)
            mDevice->Get().destroyDescriptorSetLayout(mLayout);
    }

    void BoneMatrixResources::addOwnedInterval(const std::uint32_t begin,
                                               const std::uint32_t end)
    {
        if (begin >= end || begin >= mCapacity)
            return;
        const auto e = std::min(end, mCapacity);

        std::vector<Interval> out;
        out.reserve(mGpuOwned.size() + 1);
        bool          placed = false;
        std::uint32_t b      = begin;
        std::uint32_t x      = e;
        for (const auto& iv : mGpuOwned)
        {
            if (iv.second < b)
            {
                out.push_back(iv);
                continue;
            }
            if (iv.first > x)
            {
                if (!placed)
                {
                    out.push_back({ b, x });
                    placed = true;
                }
                out.push_back(iv);
                continue;
            }
            b = std::min(b, iv.first);
            x = std::max(x, iv.second);
        }
        if (!placed)
            out.push_back({ b, x });
        mGpuOwned = std::move(out);
    }

    void BoneMatrixResources::removeOwnedInterval(const std::uint32_t begin,
                                                  const std::uint32_t end)
    {
        if (begin >= end || mGpuOwned.empty())
            return;
        const auto e = std::min(end, mCapacity);

        std::vector<Interval> out;
        out.reserve(mGpuOwned.size() + 1);
        for (const auto& iv : mGpuOwned)
        {
            if (iv.second <= begin || iv.first >= e)
            {
                out.push_back(iv);
                continue;
            }
            if (iv.first < begin)
                out.push_back({ iv.first, begin });
            if (iv.second > e)
                out.push_back({ e, iv.second });
        }
        mGpuOwned = std::move(out);
    }

    void BoneMatrixResources::MarkGpuOwnedBones(const std::uint32_t boneOffset,
                                                const std::uint32_t count)
    {
        if (count == 0 || boneOffset >= mCapacity)
            return;
        addOwnedInterval(boneOffset,
                         boneOffset + std::min(count, mCapacity - boneOffset));
    }

    void BoneMatrixResources::UnmarkGpuOwnedBones(
        const std::uint32_t boneOffset, const std::uint32_t count)
    {
        if (count == 0 || boneOffset >= mCapacity)
            return;
        removeOwnedInterval(
            boneOffset, boneOffset + std::min(count, mCapacity - boneOffset));
    }

    void BoneMatrixResources::ClearGpuOwnedBones()
    {
        mGpuOwned.clear();
    }

    void BoneMatrixResources::Upload(const std::uint32_t frameIndex,
                                     const std::span<const glm::mat4>
                                                         bones,
                                     const std::uint32_t boneOffset)
    {
        if (bones.empty() || boneOffset >= mCapacity)
            return;

        const auto count = std::min(static_cast<std::uint32_t>(bones.size()),
                                    mCapacity - boneOffset);
        if (count == 0)
            return;

        // CPU claims this span: FiF carry must not overwrite it.
        UnmarkGpuOwnedBones(boneOffset, count);

        const auto fi       = frameIndex % mFrameCount;
        const auto base     = static_cast<std::uint64_t>(BonesByteOffset(fi));
        const auto matBytes = static_cast<std::uint64_t>(sizeof(glm::mat4));
        const auto byteOff  = static_cast<std::uint64_t>(boneOffset) * matBytes;
        const auto byteCount    = static_cast<std::uint64_t>(count) * matBytes;
        const auto paletteBytes = static_cast<std::uint64_t>(PaletteBytes());

        if (!mHasUploaded)
        {
            std::memcpy(mCpuPrev.data() + boneOffset, bones.data(),
                        static_cast<std::size_t>(byteCount));
            mHasUploaded = true;
        }

        mBuffer->Copy(mCpuPrev.data() + boneOffset, byteCount,
                      base + paletteBytes + byteOff);
        mBuffer->Copy(bones.data(), byteCount, base + byteOff);

        std::memcpy(mCpuPrev.data() + boneOffset, bones.data(),
                    static_cast<std::size_t>(byteCount));
    }

    void BoneMatrixResources::recordOwnedCopies(
        const vk::CommandBuffer      commandBuffer,
        const vk::DeviceSize         srcBase,
        const vk::DeviceSize         dstBase,
        const vk::DeviceSize         barrierOffset,
        const vk::DeviceSize         barrierSize,
        const vk::PipelineStageFlags dstStages,
        const vk::AccessFlags        dstAccess) const
    {
        if (mGpuOwned.empty() || barrierSize == 0)
            return;

        const auto matBytes = static_cast<vk::DeviceSize>(sizeof(glm::mat4));

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
                .setOffset(barrierOffset)
                .setSize(barrierSize),
            {});

        std::vector<vk::BufferCopy> regions;
        regions.reserve(mGpuOwned.size());
        for (const auto& iv : mGpuOwned)
        {
            const auto bytes =
                static_cast<vk::DeviceSize>(iv.second - iv.first) * matBytes;
            const auto rel = static_cast<vk::DeviceSize>(iv.first) * matBytes;
            regions.push_back(vk::BufferCopy()
                                  .setSrcOffset(srcBase + rel)
                                  .setDstOffset(dstBase + rel)
                                  .setSize(bytes));
        }
        commandBuffer.copyBuffer(mBuffer->Get(), mBuffer->Get(), regions);

        commandBuffer.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer, dstStages, {}, {},
            vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
                .setDstAccessMask(dstAccess)
                .setBuffer(mBuffer->Get())
                .setOffset(barrierOffset)
                .setSize(barrierSize),
            {});
    }

    void BoneMatrixResources::RecordCarryBonesFromPreviousFrame(
        const vk::CommandBuffer commandBuffer,
        const std::uint32_t     frameIndex) const
    {
        if (mFrameCount < 2 || mGpuOwned.empty())
            return;

        const auto curFi  = frameIndex % mFrameCount;
        const auto prevFi = (curFi + mFrameCount - 1u) % mFrameCount;
        const auto srcOff = BonesByteOffset(prevFi);
        const auto dstOff = BonesByteOffset(curFi);
        const auto bytes  = PaletteBytes();
        const auto lo     = srcOff < dstOff ? srcOff : dstOff;
        const auto hi     = (srcOff < dstOff ? dstOff : srcOff) + bytes;

        recordOwnedCopies(
            commandBuffer, srcOff, dstOff, lo, hi - lo,
            vk::PipelineStageFlagBits::eTransfer |
                vk::PipelineStageFlagBits::eComputeShader,
            vk::AccessFlagBits::eTransferRead |
                vk::AccessFlagBits::eTransferWrite |
                vk::AccessFlagBits::eShaderRead |
                vk::AccessFlagBits::eShaderWrite);
    }

    void BoneMatrixResources::RecordCopyCurrentToPrev(
        const vk::CommandBuffer commandBuffer,
        const std::uint32_t     frameIndex) const
    {
        if (mGpuOwned.empty())
            return;

        const auto bonesOff = BonesByteOffset(frameIndex);
        const auto prevOff  = PrevBonesByteOffset(frameIndex);
        const auto bytes    = PaletteBytes();

        recordOwnedCopies(
            commandBuffer, bonesOff, prevOff, bonesOff, bytes + bytes,
            vk::PipelineStageFlagBits::eComputeShader,
            vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
    }

} // namespace FREYA_NAMESPACE
