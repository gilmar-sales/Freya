#include "Freya/Core/Limits.hpp"
#include "Freya/Core/UniformBuffer.hpp"
#include "Freya/FreyaOptions.hpp"
#include "Freya/Internal/LightServiceGpu.hpp"

#include "Freya/Builders/BufferBuilder.hpp"

#include <algorithm>

namespace FREYA_NAMESPACE
{
    LightService::LightService(
        const skr::Arc<skr::ServiceProvider>& serviceProvider) :
        mImpl(std::make_unique<Impl>())
    {
        const auto device  = serviceProvider->GetService<Device>();
        const auto options = serviceProvider->GetService<FreyaOptions>();

        mImpl->mDevice       = device;
        mImpl->mFrameCount   = options->frameCount;
        mImpl->mMaxLights    = std::min(options->maxLights, kMaxLights);
        mImpl->mLightCount   = 0;
        mImpl->mLayout       = nullptr;
        mImpl->mPool         = nullptr;
        mImpl->mIblIntensity = options->iblIntensity;
        mImpl->mExposure     = options->exposure;

        const auto bufferSize =
            sizeof(LightUniformBuffer) * options->frameCount;

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

        mImpl->mDevice->Get().waitIdle();

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

    namespace
    {
        void PackLights(const std::vector<Light>&        lights,
                        const std::vector<std::uint8_t>& alive,
                        const std::uint32_t              maxPacked,
                        const bool                       shadowsEnabled,
                        LightUniformBuffer&              data)
        {
            std::uint32_t packed = 0;
            for (std::uint32_t n = 0; n < lights.size() && packed < maxPacked;
                 ++n)
            {
                if (n >= alive.size() || !alive[n])
                    continue;
                data.lightPositions[packed] = glm::vec4(
                    lights[n].position, static_cast<float>(lights[n].type));
                data.lightColorsAndRadius[packed] =
                    glm::vec4(lights[n].color, lights[n].radius);
                data.lightDirectionsAndCutoff[packed] =
                    glm::vec4(lights[n].direction, lights[n].innerCutoff);
                data.lightOuterCutoffAndIntensity[packed] = glm::vec4(
                    lights[n].outerCutoff, lights[n].intensity,
                    lights[n].halfHeight,
                    (shadowsEnabled && lights[n].castShadows) ? 1.0f : 0.0f);
                data.lightAreaTangents[packed] =
                    glm::vec4(lights[n].tangent, 0.0f);
                ++packed;
            }
            data.lightCount = packed;
        }
    } // namespace

    LightHandle LightService::AddLight(const Light& light)
    {
        auto& i = *mImpl;
        if (i.mLightCount >= i.mMaxLights)
        {
            return {};
        }

        for (std::uint32_t n = 0; n < i.mAlive.size(); ++n)
        {
            if (!i.mAlive[n])
            {
                i.mLights[n] = light;
                i.mAlive[n]  = 1;
                ++i.mLightCount;
                return LightHandle { n };
            }
        }

        i.mLights.push_back(light);
        i.mAlive.push_back(1);
        ++i.mLightCount;
        return LightHandle { static_cast<std::uint32_t>(i.mLights.size() - 1) };
    }

    void LightService::RemoveLight(const LightHandle handle)
    {
        if (!handle)
            return;
        auto&               i     = *mImpl;
        const std::uint32_t index = handle.Index();
        if (index >= i.mAlive.size() || !i.mAlive[index])
            return;

        i.mAlive[index] = 0;
        --i.mLightCount;

        LightUniformBuffer data = {};
        data.iblIntensity       = i.mIblIntensity;
        data.exposure           = i.mExposure;
        PackLights(i.mLights, i.mAlive, i.mMaxLights, i.mShadowsEnabled, data);

        for (std::uint32_t f = 0; f < i.mFrameCount; ++f)
        {
            i.mBuffer->Copy(&data, sizeof(LightUniformBuffer),
                            f * sizeof(LightUniformBuffer));
        }
    }

    void LightService::UpdateLightPosition(const LightHandle handle,
                                           const glm::vec3&  position)
    {
        if (!handle)
            return;
        auto&               i     = *mImpl;
        const std::uint32_t index = handle.Index();
        if (index >= i.mAlive.size() || !i.mAlive[index])
            return;
        i.mLights[index].position = position;
    }

    void LightService::UpdateLight(const LightHandle handle, const Light& light)
    {
        if (!handle)
            return;
        auto&               i     = *mImpl;
        const std::uint32_t index = handle.Index();
        if (index >= i.mAlive.size() || !i.mAlive[index])
            return;
        i.mLights[index] = light;
    }

    const Light* LightService::GetLight(const LightHandle handle) const
    {
        if (!handle)
            return nullptr;
        auto&               i     = *mImpl;
        const std::uint32_t index = handle.Index();
        if (index >= i.mAlive.size() || !i.mAlive[index])
            return nullptr;
        return &i.mLights[index];
    }

    void LightService::ClearLights()
    {
        auto& i = *mImpl;
        i.mLights.clear();
        i.mAlive.clear();
        i.mLightCount = 0;

        LightUniformBuffer data = {};
        data.iblIntensity       = i.mIblIntensity;
        data.exposure           = i.mExposure;
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

        data.iblIntensity  = i.mIblIntensity;
        data.exposure      = i.mExposure;
        data.viewPosition  = glm::vec4(viewPosition, 1.0f);
        data.cameraForward = glm::vec4(cameraForward, 0.0f);
        PackLights(i.mLights, i.mAlive, i.mMaxLights, i.mShadowsEnabled, data);

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
