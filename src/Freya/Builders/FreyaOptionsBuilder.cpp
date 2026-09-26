#include "Freya/Builders/FreyaOptionsBuilder.hpp"

#include <algorithm>

namespace FREYA_NAMESPACE
{
    FreyaOptionsBuilder::FreyaOptionsBuilder() :
        mFreyaOptions(skr::MakeArc<FreyaOptions>())
    {
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetTitle(const std::string& title)
    {
        mFreyaOptions->title = title;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetWidth(std::uint32_t width)
    {
        mFreyaOptions->width = width;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetHeight(std::uint32_t height)
    {
        mFreyaOptions->height = height;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetVSync(bool vSync)
    {
        mFreyaOptions->vSync = vSync;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetFullscreen(bool fullscreen)
    {
        mFreyaOptions->fullscreen = fullscreen;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetSampleCount(
        std::uint32_t sampleCount)
    {
        mFreyaOptions->sampleCount = sampleCount;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetFrameCount(
        std::uint32_t frameCount)
    {
        mFreyaOptions->frameCount = frameCount;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetClearColor(
        const glm::vec4& clearColor)
    {
        mFreyaOptions->clearColor = clearColor;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetDrawDistance(
        float drawDistance)
    {
        mFreyaOptions->drawDistance = drawDistance;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetMaxLights(
        std::uint32_t maxLights)
    {
        mFreyaOptions->maxLights = maxLights;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetIblIntensity(float intensity)
    {
        mFreyaOptions->iblIntensity = intensity;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetExposure(float exposure)
    {
        mFreyaOptions->exposure = exposure;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetAmbient(const glm::vec3& color,
                                                         float intensity)
    {
        mFreyaOptions->ambientColor     = color;
        mFreyaOptions->ambientIntensity = intensity;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetEnvironmentMapPath(
        const std::string& path)
    {
        mFreyaOptions->environmentMapPath = path;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetShadowCascadeCount(
        std::uint32_t count)
    {
        mFreyaOptions->shadowCascadeCount = count;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetShadowMapResolution(
        std::uint32_t resolution)
    {
        mFreyaOptions->shadowMapResolution = resolution;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetShadowBias(float bias)
    {
        mFreyaOptions->shadowBias = bias;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetShadowCascadeDistance(
        float distance)
    {
        mFreyaOptions->shadowCascadeDistance = std::max(1.0f, distance);
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetShadowCascadeBlend(float blend)
    {
        mFreyaOptions->shadowCascadeBlend = std::clamp(blend, 0.0f, 0.5f);
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetShadowLightSize(
        float lightSize)
    {
        mFreyaOptions->shadowLightSize = lightSize;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetShadowMaxSoftness(
        float maxSoftness)
    {
        mFreyaOptions->shadowMaxSoftness = maxSoftness;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetShadowMinVisibility(
        float minVisibility)
    {
        mFreyaOptions->shadowMinVisibility = minVisibility;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetMaxSpotShadows(
        std::uint32_t count)
    {
        mFreyaOptions->maxSpotShadows = count;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetMaxPointShadows(
        std::uint32_t count)
    {
        mFreyaOptions->maxPointShadows = count;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetShadowPointResolution(
        std::uint32_t resolution)
    {
        mFreyaOptions->shadowPointResolution = resolution;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetShadowPointResolutionDivisor(
        std::uint32_t divisor)
    {
        mFreyaOptions->shadowPointResolutionDivisor = divisor;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetShadowSpotResolution(
        std::uint32_t resolution)
    {
        mFreyaOptions->shadowSpotResolution = resolution;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetShadowSpotResolutionDivisor(
        std::uint32_t divisor)
    {
        mFreyaOptions->shadowSpotResolutionDivisor = divisor;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetShadowPointUpdatePeriod(
        std::uint32_t period)
    {
        mFreyaOptions->shadowPointUpdatePeriod = period;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetShadowSampleCount(
        std::uint32_t count)
    {
        mFreyaOptions->shadowSampleCount = count;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetShadowQuality(
        ShadowQuality quality)
    {
        ApplyShadowQuality(*mFreyaOptions, quality);
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetSsaoQuality(
        SsaoQuality quality)
    {
        ApplySsaoQuality(*mFreyaOptions, quality);
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetTaaQuality(TaaQuality quality)
    {
        ApplyTaaQuality(*mFreyaOptions, quality);
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetBloomQuality(
        BloomQuality quality)
    {
        ApplyBloomQuality(*mFreyaOptions, quality);
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetAnimationQuality(
        AnimationQuality quality)
    {
        ApplyAnimationQuality(*mFreyaOptions, quality);
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetAnimLodEnabled(bool enabled)
    {
        mFreyaOptions->enableAnimLod = enabled;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetAnimBakeHz(float hz)
    {
        mFreyaOptions->animBakeHz = std::max(1.f, hz);
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetQuantizeGpuAnimJoints(
        bool enabled)
    {
        mFreyaOptions->quantizeGpuAnimJoints = enabled;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetSsaoResolutionDivisor(
        std::uint32_t divisor)
    {
        mFreyaOptions->ssaoResolutionDivisor = std::max(1u, divisor);
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetSsaoRadius(float radius)
    {
        mFreyaOptions->ssaoRadius = radius;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetSsaoBias(float bias)
    {
        mFreyaOptions->ssaoBias = bias;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetSsaoPower(float power)
    {
        mFreyaOptions->ssaoPower = power;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetDeferredDebugView(
        DeferredDebugView view)
    {
        mFreyaOptions->deferredDebugView = view;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetSsaoIntensity(float intensity)
    {
        mFreyaOptions->ssaoIntensity = intensity;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetTaaCurrentWeight(float weight)
    {
        mFreyaOptions->taaCurrentWeight = weight;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetTaaHaltonPeriod(
        std::uint32_t period)
    {
        mFreyaOptions->taaHaltonPeriod = std::max(1u, period);
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetTaaVarianceGammaY(float gamma)
    {
        mFreyaOptions->taaVarianceGammaY = gamma;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetTaaVarianceGammaC(float gamma)
    {
        mFreyaOptions->taaVarianceGammaC = gamma;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetTaaDepthRejectThreshold(
        float threshold)
    {
        mFreyaOptions->taaDepthRejectThreshold = threshold;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetTaaSharpen(float sharpen)
    {
        mFreyaOptions->taaSharpen = sharpen;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetBloomResolutionDivisor(
        std::uint32_t divisor)
    {
        mFreyaOptions->bloomResolutionDivisor = std::max(1u, divisor);
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetBloomThreshold(float threshold)
    {
        mFreyaOptions->bloomThreshold = threshold;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetBloomExtractScale(float scale)
    {
        mFreyaOptions->bloomExtractScale = scale;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetBloomStrength(float strength)
    {
        mFreyaOptions->bloomStrength = strength;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::WithReverseZ(bool value)
    {
        mFreyaOptions->ReverseZ = value;

        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetShaderRoot(
        const std::string& shaderRoot)
    {
        mFreyaOptions->shaderRoot = shaderRoot;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetEnableShadows(bool enable)
    {
        mFreyaOptions->enableShadows = enable;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetEnableSsao(bool enable)
    {
        mFreyaOptions->enableSsao = enable;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetEnableTaa(bool enable)
    {
        mFreyaOptions->enableTaa = enable;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetEnableBloom(bool enable)
    {
        mFreyaOptions->enableBloom = enable;
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetMeshLodPixelRef(float pixels)
    {
        mFreyaOptions->meshLodPixelRef = std::max(1.0f, pixels);
        return *this;
    }

    FreyaOptionsBuilder& FreyaOptionsBuilder::SetMeshLodStep(float step)
    {
        mFreyaOptions->meshLodStep = std::max(1.01f, step);
        return *this;
    }

    skr::Arc<FreyaOptions> FreyaOptionsBuilder::Build()
    {
        return mFreyaOptions;
    }

} // namespace FREYA_NAMESPACE