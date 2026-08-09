#include "Freya/Asset/BoneMatrixResources.hpp"

#include "Freya/Builders/BufferBuilder.hpp"

#include <algorithm>
#include <cstring>

namespace FREYA_NAMESPACE
{
    BoneMatrixResources::BoneMatrixResources(const skr::Arc<Device>& device,
                                             const std::uint32_t     frameCount,
                                             const std::uint32_t capacity) :
        mDevice(device), mFrameCount(std::max(1u, frameCount)),
        mCapacity(std::max(1u, capacity)),
        mCpuPrev(mCapacity, glm::mat4(1.f))
    {
        const auto totalSize = frameBytes() * mFrameCount;
        mBuffer              = BufferBuilder(mDevice)
                      .SetUsage(BufferUsage::Storage)
                      .SetSize(static_cast<std::uint32_t>(totalSize))
                      .Build();

        {
            std::vector<glm::mat4> id(mCapacity * 2u * mFrameCount,
                                      glm::mat4(1.f));
            mBuffer->Copy(id.data(),
                          static_cast<std::uint32_t>(id.size() *
                                                     sizeof(glm::mat4)));
        }

        const auto bindings = std::array {
            vk::DescriptorSetLayoutBinding()
                .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eVertex),
            vk::DescriptorSetLayoutBinding()
                .setBinding(1)
                .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eVertex),
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

        const auto paletteBytes =
            static_cast<vk::DeviceSize>(mCapacity) * sizeof(glm::mat4);
        for (std::uint32_t i = 0; i < mFrameCount; ++i)
        {
            const auto base = static_cast<vk::DeviceSize>(i) * frameBytes();
            auto       bonesInfo =
                vk::DescriptorBufferInfo()
                    .setBuffer(mBuffer->Get())
                    .setOffset(base)
                    .setRange(paletteBytes);
            auto prevInfo =
                vk::DescriptorBufferInfo()
                    .setBuffer(mBuffer->Get())
                    .setOffset(base + paletteBytes)
                    .setRange(paletteBytes);
            const auto writes = std::array {
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

    void BoneMatrixResources::Upload(const std::uint32_t frameIndex,
                                     const std::span<const glm::mat4> bones)
    {
        const auto count =
            std::min(static_cast<std::uint32_t>(bones.size()), mCapacity);
        const auto fi   = frameIndex % mFrameCount;
        const auto base = static_cast<std::uint64_t>(fi) *
                          static_cast<std::uint64_t>(frameBytes());
        const auto paletteBytes =
            static_cast<std::uint64_t>(mCapacity) * sizeof(glm::mat4);

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

} // namespace FREYA_NAMESPACE
