#include "Freya/Core/LightService.hpp"

#include "Freya/Builders/BufferBuilder.hpp"

namespace FREYA_NAMESPACE
{
    LightService::LightService(const Ref<Device>& device,
                               std::uint32_t      frameCount,
                               std::uint32_t      maxLights) :
        mDevice(device), mFrameCount(frameCount), mMaxLights(maxLights),
        mLightCount(0), mLights(), mLayout(nullptr), mPool(nullptr)
    {
        // Create light buffer with std140 layout
        const auto bufferSize = sizeof(LightUniformBuffer) * frameCount;

        mBuffer = BufferBuilder(mDevice)
                      .SetUsage(BufferUsage::Uniform)
                      .SetSize(bufferSize)
                      .Build();

        createDescriptorResources();
    }

    LightService::~LightService()
    {
        auto& vkDevice = mDevice->Get();

        if (mPool)
        {
            vkDevice.destroyDescriptorPool(mPool);
        }

        if (mLayout)
        {
            vkDevice.destroyDescriptorSetLayout(mLayout);
        }
    }

    LightService::LightService(LightService&& other) noexcept :
        mDevice(other.mDevice), mFrameCount(other.mFrameCount),
        mMaxLights(other.mMaxLights), mLightCount(other.mLightCount),
        mLights(std::move(other.mLights)), mBuffer(other.mBuffer),
        mLayout(other.mLayout), mPool(other.mPool),
        mSets(std::move(other.mSets))
    {
        other.mLayout = nullptr;
        other.mPool   = nullptr;
    }

    LightService& LightService::operator=(LightService&& other) noexcept
    {
        if (this != &other)
        {
            mDevice     = other.mDevice;
            mFrameCount = other.mFrameCount;
            mMaxLights  = other.mMaxLights;
            mLightCount = other.mLightCount;
            mLights     = std::move(other.mLights);
            mBuffer     = other.mBuffer;
            mLayout     = other.mLayout;
            mPool       = other.mPool;
            mSets       = std::move(other.mSets);

            other.mLayout = nullptr;
            other.mPool   = nullptr;
        }
        return *this;
    }

    std::int32_t LightService::AddLight(const Light& light)
    {
        if (mLightCount >= mMaxLights)
        {
            return -1;
        }

        mLights.push_back(light);
        mLightCount++;

        return static_cast<std::int32_t>(mLights.size() - 1);
    }

    void LightService::RemoveLight(const std::uint32_t index)
    {
        if (index >= mLights.size())
        {
            return;
        }

        mLights.erase(mLights.begin() + index);
        mLightCount--;

        // Update buffer after removal
        LightUniformBuffer data = {};
        for (std::uint32_t i = 0; i < mLightCount; ++i)
        {
            data.lightPositions[i] =
                glm::vec4(mLights[i].position, mLights[i].type);
            data.lightColorsAndRadius[i] =
                glm::vec4(mLights[i].color, mLights[i].radius);
            data.lightDirectionsAndCutoff[i] =
                glm::vec4(mLights[i].direction, mLights[i].innerCutoff);
            data.lightOuterCutoffAndIntensity[i] =
                glm::vec4(mLights[i].outerCutoff,
                          mLights[i].intensity,
                          0.0f,
                          0.0f);
        }

        for (std::uint32_t f = 0; f < mFrameCount; ++f)
        {
            mBuffer->Copy(&data,
                          sizeof(LightUniformBuffer),
                          f * sizeof(LightUniformBuffer));
        }
    }

    void LightService::UpdateLightPosition(std::uint32_t    index,
                                           const glm::vec3& position)
    {
        if (index >= mLights.size())
        {
            return;
        }

        mLights[index].position = position;
    }

    void LightService::ClearLights()
    {
        mLights.clear();
        mLightCount = 0;

        // Clear buffer
        LightUniformBuffer data = {};
        for (std::uint32_t f = 0; f < mFrameCount; ++f)
        {
            mBuffer->Copy(&data,
                          sizeof(LightUniformBuffer),
                          f * sizeof(LightUniformBuffer));
        }
    }

    void LightService::Update(std::uint32_t    frameIndex,
                              const glm::vec3& viewPosition)
    {
        LightUniformBuffer data = {};

        data.lightCount   = mLightCount;
        data.viewPosition = glm::vec4(viewPosition, 1.0f);

        for (std::uint32_t i = 0; i < mLightCount; ++i)
        {
            data.lightPositions[i] =
                glm::vec4(mLights[i].position, mLights[i].type);
            data.lightColorsAndRadius[i] =
                glm::vec4(mLights[i].color, mLights[i].radius);
            data.lightDirectionsAndCutoff[i] =
                glm::vec4(mLights[i].direction, mLights[i].innerCutoff);
            data.lightOuterCutoffAndIntensity[i] =
                glm::vec4(mLights[i].outerCutoff,
                          mLights[i].intensity,
                          0.0f,
                          0.0f);
        }

        // Copy to ring-buffer offset for this frame
        const auto offset = frameIndex * sizeof(LightUniformBuffer);
        mBuffer->Copy(&data, sizeof(LightUniformBuffer), offset);

        // Update descriptor sets with correct offset
        auto bufferInfo =
            vk::DescriptorBufferInfo()
                .setBuffer(mBuffer->Get())
                .setOffset(offset)
                .setRange(sizeof(LightUniformBuffer));

        auto writer =
            vk::WriteDescriptorSet()
                .setDstSet(mSets[frameIndex])
                .setDstBinding(0)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setBufferInfo(bufferInfo);

        mDevice->Get().updateDescriptorSets(1, &writer, 0, nullptr);
    }

    void LightService::createDescriptorResources()
    {
        auto& vkDevice = mDevice->Get();

        // Create descriptor set layout
        auto binding =
            vk::DescriptorSetLayoutBinding()
                .setBinding(0)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setStageFlags(vk::ShaderStageFlagBits::eVertex |
                               vk::ShaderStageFlagBits::eFragment);

        auto layoutInfo =
            vk::DescriptorSetLayoutCreateInfo().setBindings(binding);

        mLayout = vkDevice.createDescriptorSetLayout(layoutInfo);

        // Create descriptor pool
        auto poolSize = vk::DescriptorPoolSize()
                            .setType(vk::DescriptorType::eUniformBuffer)
                            .setDescriptorCount(mFrameCount);

        auto poolInfo = vk::DescriptorPoolCreateInfo()
                            .setPoolSizeCount(1)
                            .setPPoolSizes(&poolSize)
                            .setMaxSets(mFrameCount);

        mPool = vkDevice.createDescriptorPool(poolInfo);

        // Allocate descriptor sets
        auto layouts =
            std::vector<vk::DescriptorSetLayout>(mFrameCount, mLayout);

        auto allocInfo = vk::DescriptorSetAllocateInfo()
                             .setDescriptorPool(mPool)
                             .setSetLayouts(layouts);

        mSets = vkDevice.allocateDescriptorSets(allocInfo);

        // Initial update for all sets
        for (std::uint32_t i = 0; i < mFrameCount; ++i)
        {
            auto bufferInfo =
                vk::DescriptorBufferInfo()
                    .setBuffer(mBuffer->Get())
                    .setOffset(i * sizeof(LightUniformBuffer))
                    .setRange(sizeof(LightUniformBuffer));

            auto writer =
                vk::WriteDescriptorSet()
                    .setDstSet(mSets[i])
                    .setDstBinding(0)
                    .setDstArrayElement(0)
                    .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(bufferInfo);

            vkDevice.updateDescriptorSets(1, &writer, 0, nullptr);
        }
    }

} // namespace FREYA_NAMESPACE