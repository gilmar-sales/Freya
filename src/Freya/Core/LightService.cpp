#include "Freya/Core/Limits.hpp"
#include "Freya/Core/UniformBuffer.hpp"
#include "Freya/Internal/LightServiceGpu.hpp"

#include "Freya/Builders/BufferBuilder.hpp"

#include <algorithm>

namespace FREYA_NAMESPACE
{
    LightService::LightService(const skr::Arc<Device>& device,
                               std::uint32_t           frameCount,
                               std::uint32_t           maxLights) :
        mImpl(std::make_unique<Impl>())
    {
        mImpl->mDevice     = device;
        mImpl->mFrameCount = frameCount;
        mImpl->mMaxLights  = std::min(maxLights, kMaxLights);
        mImpl->mLightCount = 0;
        mImpl->mLayout     = nullptr;
        mImpl->mPool       = nullptr;

        const auto bufferSize = sizeof(LightUniformBuffer) * frameCount;

        mImpl->mBuffer = BufferBuilder(mImpl->mDevice)
                             .SetUsage(BufferUsage::Uniform)
                             .SetSize(bufferSize)
                             .Build();

        mImpl->createDescriptorResources();
    }

    LightService::~LightService()
    {
        if (!mImpl || !mImpl->mDevice)
            return;

        auto& vkDevice = mImpl->mDevice->Get();

        if (mImpl->mPool)
        {
            vkDevice.destroyDescriptorPool(mImpl->mPool);
        }

        if (mImpl->mLayout)
        {
            vkDevice.destroyDescriptorSetLayout(mImpl->mLayout);
        }
    }

    LightService::LightService(LightService&& other) noexcept = default;
    LightService& LightService::operator=(LightService&& other) noexcept =
        default;

    std::int32_t LightService::AddLight(const Light& light)
    {
        auto& i = *mImpl;
        if (i.mLightCount >= i.mMaxLights)
        {
            return -1;
        }

        i.mLights.push_back(light);
        i.mLightCount++;

        return static_cast<std::int32_t>(i.mLights.size() - 1);
    }

    void LightService::RemoveLight(const std::uint32_t index)
    {
        auto& i = *mImpl;
        if (index >= i.mLights.size())
        {
            return;
        }

        i.mLights.erase(i.mLights.begin() + index);
        i.mLightCount--;

        LightUniformBuffer data = {};
        for (std::uint32_t n = 0; n < i.mLightCount; ++n)
        {
            data.lightPositions[n] =
                glm::vec4(i.mLights[n].position, i.mLights[n].type);
            data.lightColorsAndRadius[n] =
                glm::vec4(i.mLights[n].color, i.mLights[n].radius);
            data.lightDirectionsAndCutoff[n] =
                glm::vec4(i.mLights[n].direction, i.mLights[n].innerCutoff);
            data.lightOuterCutoffAndIntensity[n] = glm::vec4(
                i.mLights[n].outerCutoff,
                i.mLights[n].intensity,
                i.mLights[n].halfHeight,
                (i.mShadowsEnabled && i.mLights[n].castShadows) ? 1.0f : 0.0f);
            data.lightAreaTangents[n] = glm::vec4(i.mLights[n].tangent, 0.0f);
        }

        for (std::uint32_t f = 0; f < i.mFrameCount; ++f)
        {
            i.mBuffer->Copy(&data, sizeof(LightUniformBuffer),
                            f * sizeof(LightUniformBuffer));
        }
    }

    void LightService::UpdateLightPosition(std::uint32_t    index,
                                           const glm::vec3& position)
    {
        auto& i = *mImpl;
        if (index >= i.mLights.size())
            return;
        i.mLights[index].position = position;
    }

    void LightService::UpdateLight(std::uint32_t index, const Light& light)
    {
        auto& i = *mImpl;
        if (index >= i.mLights.size())
            return;
        i.mLights[index] = light;
    }

    const Light* LightService::GetLight(const std::uint32_t index) const
    {
        auto& i = *mImpl;
        if (index >= i.mLights.size())
            return nullptr;
        return &i.mLights[index];
    }

    void LightService::ClearLights()
    {
        auto& i = *mImpl;
        i.mLights.clear();
        i.mLightCount = 0;

        LightUniformBuffer data = {};
        for (std::uint32_t f = 0; f < i.mFrameCount; ++f)
        {
            i.mBuffer->Copy(&data, sizeof(LightUniformBuffer),
                            f * sizeof(LightUniformBuffer));
        }
    }

    void LightService::Update(std::uint32_t    frameIndex,
                              const glm::vec3& viewPosition,
                              const glm::vec3& cameraForward)
    {
        auto&              i    = *mImpl;
        LightUniformBuffer data = {};

        data.lightCount    = i.mLightCount;
        data.iblIntensity  = i.mIblIntensity;
        data.exposure      = i.mExposure;
        data.viewPosition  = glm::vec4(viewPosition, 1.0f);
        data.cameraForward = glm::vec4(cameraForward, 0.0f);

        for (std::uint32_t n = 0; n < i.mLightCount; ++n)
        {
            data.lightPositions[n] =
                glm::vec4(i.mLights[n].position, i.mLights[n].type);
            data.lightColorsAndRadius[n] =
                glm::vec4(i.mLights[n].color, i.mLights[n].radius);
            data.lightDirectionsAndCutoff[n] =
                glm::vec4(i.mLights[n].direction, i.mLights[n].innerCutoff);
            data.lightOuterCutoffAndIntensity[n] = glm::vec4(
                i.mLights[n].outerCutoff,
                i.mLights[n].intensity,
                i.mLights[n].halfHeight,
                (i.mShadowsEnabled && i.mLights[n].castShadows) ? 1.0f : 0.0f);
            data.lightAreaTangents[n] = glm::vec4(i.mLights[n].tangent, 0.0f);
        }

        const auto offset = frameIndex * sizeof(LightUniformBuffer);
        i.mBuffer->Copy(&data, sizeof(LightUniformBuffer), offset);
    }

    std::uint32_t LightService::GetLightCount() const
    {
        return mImpl->mLightCount;
    }

    std::uint32_t LightService::GetMaxLights() const
    {
        return mImpl->mMaxLights;
    }

    bool LightService::HasLights() const
    {
        return mImpl->mLightCount > 0;
    }

    void LightService::SetIblIntensity(const float intensity)
    {
        mImpl->mIblIntensity = intensity;
    }

    float LightService::GetIblIntensity() const
    {
        return mImpl->mIblIntensity;
    }

    void LightService::SetExposure(const float exposure)
    {
        mImpl->mExposure = exposure;
    }

    float LightService::GetExposure() const
    {
        return mImpl->mExposure;
    }

    void LightService::SetShadowsEnabled(const bool enabled)
    {
        mImpl->mShadowsEnabled = enabled;
    }

    bool LightService::GetShadowsEnabled() const
    {
        return mImpl->mShadowsEnabled;
    }

    void LightService::Impl::createDescriptorResources()
    {
        auto& vkDevice = mDevice->Get();

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

        auto poolSize = vk::DescriptorPoolSize()
                            .setType(vk::DescriptorType::eUniformBuffer)
                            .setDescriptorCount(mFrameCount);

        auto poolInfo = vk::DescriptorPoolCreateInfo()
                            .setPoolSizeCount(1)
                            .setPPoolSizes(&poolSize)
                            .setMaxSets(mFrameCount);

        mPool = vkDevice.createDescriptorPool(poolInfo);

        auto layouts =
            std::vector<vk::DescriptorSetLayout>(mFrameCount, mLayout);

        auto allocInfo = vk::DescriptorSetAllocateInfo()
                             .setDescriptorPool(mPool)
                             .setSetLayouts(layouts);

        mSets = vkDevice.allocateDescriptorSets(allocInfo);

        for (std::uint32_t n = 0; n < mFrameCount; ++n)
        {
            auto bufferInfo =
                vk::DescriptorBufferInfo()
                    .setBuffer(mBuffer->Get())
                    .setOffset(n * sizeof(LightUniformBuffer))
                    .setRange(sizeof(LightUniformBuffer));

            auto writer =
                vk::WriteDescriptorSet()
                    .setDstSet(mSets[n])
                    .setDstBinding(0)
                    .setDstArrayElement(0)
                    .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                    .setDescriptorCount(1)
                    .setBufferInfo(bufferInfo);

            vkDevice.updateDescriptorSets(1, &writer, 0, nullptr);
        }
    }

} // namespace FREYA_NAMESPACE
