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
                        const bool                       typeEnabled[4],
                        LightUniformBuffer&              data)
        {
            std::uint32_t packed = 0;
            for (std::uint32_t n = 0; n < lights.size() && packed < maxPacked;
                 ++n)
            {
                if (n >= alive.size() || !alive[n])
                    continue;
                const auto typeIndex =
                    static_cast<std::uint32_t>(lights[n].type);
                const bool  typeOn = typeIndex < 4u && typeEnabled[typeIndex];
                const bool  on     = typeOn && lights[n].enabled;
                const float intensity       = on ? lights[n].intensity : 0.0f;
                data.lightPositions[packed] = glm::vec4(
                    lights[n].position, static_cast<float>(lights[n].type));
                data.lightColorsAndRadius[packed] =
                    glm::vec4(lights[n].color, lights[n].radius);
                data.lightDirectionsAndCutoff[packed] =
                    glm::vec4(lights[n].direction, lights[n].innerCutoff);
                data.lightOuterCutoffAndIntensity[packed] = glm::vec4(
                    lights[n].outerCutoff, intensity, lights[n].halfHeight,
                    (on && shadowsEnabled && lights[n].castShadows) ? 1.0f
                                                                    : 0.0f);
                data.lightAreaTangents[packed] =
                    glm::vec4(lights[n].tangent, 0.0f);
                ++packed;
            }
            data.lightCount = packed;
        }
    } // namespace

    void LightService::applyLightUpdate(const LightHandle handle,
                                        const Light&      light)
    {
        auto&               i     = *mImpl;
        const std::uint32_t index = handle.Index();
        if (index >= i.mAlive.size() || !i.mAlive[index])
            return;
        i.mLights[index] = light;
    }

    void LightService::enqueueOrApplyUpdate(const LightHandle handle,
                                            const Light&      light)
    {
        auto& i = *mImpl;
        if (i.mStagingOpen)
        {
            const LightUpload upload { handle, light };
            UploadLightUploads(std::span<const LightUpload>(&upload, 1));
            return;
        }
        SpinLockGuard lock(i.mLock);
        applyLightUpdate(handle, light);
    }

    void LightService::mutateLight(const LightHandle                  handle,
                                   const std::function<void(Light&)>& mutate)
    {
        if (!handle)
            return;
        auto& i = *mImpl;
        if (i.mStagingOpen)
        {
            Light light {};
            {
                SpinLockGuard       lock(i.mLock);
                const std::uint32_t index = handle.Index();
                if (index >= i.mAlive.size() || !i.mAlive[index])
                    return;
                light = i.mLights[index];
                mutate(light);
            }
            enqueueOrApplyUpdate(handle, light);
            return;
        }

        SpinLockGuard       lock(i.mLock);
        const std::uint32_t index = handle.Index();
        if (index >= i.mAlive.size() || !i.mAlive[index])
            return;
        mutate(i.mLights[index]);
    }

    LightHandle LightService::AddLight(const Light& light)
    {
        SpinLockGuard lock(mImpl->mLock);
        auto&         i = *mImpl;
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
        SpinLockGuard       lock(mImpl->mLock);
        auto&               i     = *mImpl;
        const std::uint32_t index = handle.Index();
        if (index >= i.mAlive.size() || !i.mAlive[index])
            return;

        i.mAlive[index] = 0;
        --i.mLightCount;

        LightUniformBuffer data = {};
        data.iblIntensity       = i.mIblIntensity;
        data.exposure           = i.mExposure;
        PackLights(i.mLights, i.mAlive, i.mMaxLights, i.mShadowsEnabled,
                   i.mTypeEnabled, data);

        for (std::uint32_t f = 0; f < i.mFrameCount; ++f)
        {
            i.mBuffer->Copy(&data, sizeof(LightUniformBuffer),
                            f * sizeof(LightUniformBuffer));
        }
    }

    void LightService::UpdateLightPosition(const LightHandle handle,
                                           const glm::vec3&  position)
    {
        mutateLight(handle, [&](Light& light) { light.position = position; });
    }

    void LightService::SetLightColor(const LightHandle handle,
                                     const glm::vec3&  color)
    {
        mutateLight(handle, [&](Light& light) { light.color = color; });
    }

    void LightService::SetLightIntensity(const LightHandle handle,
                                         const float       intensity)
    {
        mutateLight(handle, [&](Light& light) { light.intensity = intensity; });
    }

    void LightService::SetLightDirection(const LightHandle handle,
                                         const glm::vec3&  direction)
    {
        mutateLight(handle, [&](Light& light) {
            const auto len2 = glm::dot(direction, direction);
            if (len2 > 1e-12f)
                light.direction = direction * glm::inversesqrt(len2);
        });
    }

    void LightService::SetLightRadius(const LightHandle handle,
                                      const float       radius)
    {
        mutateLight(handle, [&](Light& light) { light.radius = radius; });
    }

    void LightService::SetLightCastShadows(const LightHandle handle,
                                           const bool        castShadows)
    {
        mutateLight(handle,
                    [&](Light& light) { light.castShadows = castShadows; });
    }

    void LightService::UpdateLight(const LightHandle handle, const Light& light)
    {
        if (!handle)
            return;
        enqueueOrApplyUpdate(handle, light);
    }

    const Light* LightService::GetLight(const LightHandle handle) const
    {
        if (!handle)
            return nullptr;
        // Pointer valid until Remove/Clear; callers must not race those.
        auto&               i     = *mImpl;
        const std::uint32_t index = handle.Index();
        SpinLockGuard       lock(i.mLock);
        if (index >= i.mAlive.size() || !i.mAlive[index])
            return nullptr;
        return &i.mLights[index];
    }

    void LightService::ClearLights()
    {
        SpinLockGuard lock(mImpl->mLock);
        auto&         i = *mImpl;
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

    void LightService::BeginLightUploads()
    {
        auto& i        = *mImpl;
        i.mStagingOpen = true;
        i.mStagingCount.store(0, std::memory_order_relaxed);
    }

    void LightService::ReserveLightUploads(const std::uint32_t count)
    {
        SpinLockGuard lock(mImpl->mLock);
        if (mImpl->mStaging.size() < count)
            mImpl->mStaging.resize(count);
    }

    void LightService::UploadLightUploads(
        const std::span<const LightUpload> uploads)
    {
        const auto n = static_cast<std::uint32_t>(uploads.size());
        if (n == 0)
            return;

        auto&      i = *mImpl;
        const auto base =
            i.mStagingCount.fetch_add(n, std::memory_order_relaxed);

        SpinLockGuard lock(i.mLock);
        if (base + n > i.mStaging.size())
        {
            const auto grown = std::max(
                base + n,
                std::max<std::uint32_t>(
                    1u, static_cast<std::uint32_t>(i.mStaging.size()) * 2u));
            i.mStaging.resize(grown);
        }
        std::copy(uploads.begin(), uploads.end(),
                  i.mStaging.begin() + static_cast<std::ptrdiff_t>(base));
    }

    void LightService::EndLightUploads()
    {
        auto& i        = *mImpl;
        i.mStagingOpen = false;

        SpinLockGuard lock(i.mLock);
        const auto    count = i.mStagingCount.load(std::memory_order_relaxed);
        const auto    n =
            std::min(count, static_cast<std::uint32_t>(i.mStaging.size()));
        for (std::uint32_t u = 0; u < n; ++u)
        {
            const auto& upload = i.mStaging[u];
            if (!upload.handle)
                continue;
            applyLightUpdate(upload.handle, upload.light);
        }
        i.mStagingCount.store(0, std::memory_order_relaxed);
    }

    void LightService::Update(std::uint32_t    frameIndex,
                              const glm::vec3& viewPosition,
                              const glm::vec3& cameraForward)
    {
        SpinLockGuard      lock(mImpl->mLock);
        auto&              i    = *mImpl;
        LightUniformBuffer data = {};

        data.iblIntensity  = i.mIblIntensity;
        data.exposure      = i.mExposure;
        data.viewPosition  = glm::vec4(viewPosition, 1.0f);
        data.cameraForward = glm::vec4(cameraForward, 0.0f);
        PackLights(i.mLights, i.mAlive, i.mMaxLights, i.mShadowsEnabled,
                   i.mTypeEnabled, data);

        const auto offset = frameIndex * sizeof(LightUniformBuffer);
        i.mBuffer->Copy(&data, sizeof(LightUniformBuffer), offset);
    }

    std::uint32_t LightService::GetLightCount() const
    {
        SpinLockGuard lock(mImpl->mLock);
        return mImpl->mLightCount;
    }

    std::uint32_t LightService::GetMaxLights() const
    {
        return mImpl->mMaxLights;
    }

    bool LightService::HasLights() const
    {
        SpinLockGuard lock(mImpl->mLock);
        return mImpl->mLightCount > 0;
    }

    void LightService::SetIblIntensity(const float intensity)
    {
        SpinLockGuard lock(mImpl->mLock);
        mImpl->mIblIntensity = intensity;
    }

    float LightService::GetIblIntensity() const
    {
        SpinLockGuard lock(mImpl->mLock);
        return mImpl->mIblIntensity;
    }

    void LightService::SetExposure(const float exposure)
    {
        SpinLockGuard lock(mImpl->mLock);
        mImpl->mExposure = exposure;
    }

    float LightService::GetExposure() const
    {
        SpinLockGuard lock(mImpl->mLock);
        return mImpl->mExposure;
    }

    void LightService::SetShadowsEnabled(const bool enabled)
    {
        SpinLockGuard lock(mImpl->mLock);
        mImpl->mShadowsEnabled = enabled;
    }

    bool LightService::GetShadowsEnabled() const
    {
        SpinLockGuard lock(mImpl->mLock);
        return mImpl->mShadowsEnabled;
    }

    void LightService::SetLightTypeEnabled(const LightType type,
                                           const bool      enabled)
    {
        const auto index = static_cast<std::uint32_t>(type);
        if (index >= 4u)
            return;
        SpinLockGuard lock(mImpl->mLock);
        mImpl->mTypeEnabled[index] = enabled;
    }

    bool LightService::IsLightTypeEnabled(const LightType type) const
    {
        const auto index = static_cast<std::uint32_t>(type);
        if (index >= 4u)
            return false;
        SpinLockGuard lock(mImpl->mLock);
        return mImpl->mTypeEnabled[index];
    }

    void LightService::SetLightEnabled(const LightHandle handle,
                                       const bool        enabled)
    {
        mutateLight(handle, [&](Light& light) { light.enabled = enabled; });
    }

    bool LightService::IsLightEnabled(const LightHandle handle) const
    {
        if (!handle)
            return false;
        SpinLockGuard       lock(mImpl->mLock);
        auto&               i     = *mImpl;
        const std::uint32_t index = handle.Index();
        if (index >= i.mAlive.size() || !i.mAlive[index])
            return false;
        return i.mLights[index].enabled;
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
