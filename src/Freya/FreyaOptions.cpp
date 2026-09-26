#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{

    void ApplyShadowQuality(FreyaOptions& options, ShadowQuality quality)
    {
        if (quality == ShadowQuality::Off)
        {
            options.enableShadows = false;
            return;
        }

        options.enableShadows = true;
        switch (quality)
        {
            case ShadowQuality::Low:
                options.shadowMapResolution          = 512;
                options.shadowCascadeCount           = 2;
                options.maxSpotShadows               = 2;
                options.maxPointShadows              = 2;
                options.shadowSampleCount            = 4;
                options.shadowCascadeBlend           = 0.0f;
                options.shadowCascadeDistance        = 40.0f;
                options.shadowPointResolution        = 0;
                options.shadowPointResolutionDivisor = 2;
                options.shadowSpotResolution         = 0;
                options.shadowSpotResolutionDivisor  = 2;
                options.shadowPointUpdatePeriod      = 2;
                options.shadowCascadeUpdatePeriod    = 2;
                options.shadowMaskResolutionDivisor  = 2;
                options.enableShadowMask             = false;
                break;
            case ShadowQuality::Medium:
                options.shadowMapResolution          = 1024;
                options.shadowCascadeCount           = 3;
                options.maxSpotShadows               = 4;
                options.maxPointShadows              = 2;
                options.shadowSampleCount            = 8;
                options.shadowCascadeBlend           = 0.0f;
                options.shadowCascadeDistance        = 60.0f;
                options.shadowPointResolution        = 0;
                options.shadowPointResolutionDivisor = 2;
                options.shadowSpotResolution         = 0;
                options.shadowSpotResolutionDivisor  = 2;
                options.shadowPointUpdatePeriod      = 2;
                options.shadowCascadeUpdatePeriod    = 2;
                options.shadowMaskResolutionDivisor  = 2;
                options.enableShadowMask             = false;
                break;
            case ShadowQuality::High:
                options.shadowMapResolution          = 2048;
                options.shadowCascadeCount           = 4;
                options.maxSpotShadows               = 4;
                options.maxPointShadows              = 2;
                options.shadowSampleCount            = 16;
                options.shadowCascadeBlend           = 0.05f;
                options.shadowCascadeDistance        = 80.0f;
                options.shadowPointResolution        = 0;
                options.shadowPointResolutionDivisor = 1;
                options.shadowSpotResolution         = 0;
                options.shadowSpotResolutionDivisor  = 1;
                options.shadowPointUpdatePeriod      = 2;
                options.shadowCascadeUpdatePeriod    = 2;
                options.shadowMaskResolutionDivisor  = 1;
                options.enableShadowMask             = false;
                break;
            case ShadowQuality::Ultra:
                options.shadowMapResolution          = 4096;
                options.shadowCascadeCount           = 4;
                options.maxSpotShadows               = 4;
                options.maxPointShadows              = 2;
                options.shadowSampleCount            = 16;
                options.shadowCascadeBlend           = 0.3f;
                options.shadowCascadeDistance        = 120.0f;
                options.shadowPointResolution        = 0;
                options.shadowPointResolutionDivisor = 1;
                options.shadowSpotResolution         = 0;
                options.shadowSpotResolutionDivisor  = 1;
                options.shadowPointUpdatePeriod      = 1;
                options.shadowCascadeUpdatePeriod    = 1;
                options.shadowMaskResolutionDivisor  = 1;
                options.enableShadowMask             = false;
                break;
            case ShadowQuality::Off:
                break;
        }
    }

    void ApplySsaoQuality(FreyaOptions& options, SsaoQuality quality)
    {
        if (quality == SsaoQuality::Off)
        {
            options.enableSsao = false;
            return;
        }

        options.enableSsao = true;
        switch (quality)
        {
            case SsaoQuality::Low:
                // Half-res (quarter-res reads as blocky after upsample).
                options.ssaoResolutionDivisor = 2;
                options.ssaoRadius            = 0.4f;
                options.ssaoBias              = 0.03f;
                options.ssaoPower             = 1.4f;
                options.ssaoIntensity         = 0.5f;
                break;
            case SsaoQuality::Medium:
                options.ssaoResolutionDivisor = 2;
                options.ssaoRadius            = 0.5f;
                options.ssaoBias              = 0.025f;
                options.ssaoPower             = 1.5f;
                options.ssaoIntensity         = 0.5f;
                break;
            case SsaoQuality::High:
                options.ssaoResolutionDivisor = 2;
                options.ssaoRadius            = 0.65f;
                options.ssaoBias              = 0.025f;
                options.ssaoPower             = 1.6f;
                options.ssaoIntensity         = 0.5f;
                break;
            case SsaoQuality::Ultra:
                options.ssaoResolutionDivisor = 1;
                options.ssaoRadius            = 0.8f;
                options.ssaoBias              = 0.02f;
                options.ssaoPower             = 1.7f;
                options.ssaoIntensity         = 0.5f;
                break;
            case SsaoQuality::Off:
                break;
        }
    }

    void ApplyTaaQuality(FreyaOptions& options, TaaQuality quality)
    {
        if (quality == TaaQuality::Off)
        {
            options.enableTaa = false;
            return;
        }

        options.enableTaa = true;
        switch (quality)
        {
            case TaaQuality::Low:
                options.taaQualityLevel         = 0;
                options.taaCurrentWeight        = 0.25f;
                options.taaHaltonPeriod         = 4;
                options.taaVarianceGammaY       = 1.15f;
                options.taaVarianceGammaC       = 1.15f;
                options.taaDepthRejectThreshold = 1.0f; // unused
                options.taaSharpen              = 0.0f;
                break;
            case TaaQuality::Medium:
                options.taaQualityLevel         = 1;
                options.taaCurrentWeight        = 0.12f;
                options.taaHaltonPeriod         = 8;
                options.taaVarianceGammaY       = 1.25f;
                options.taaVarianceGammaC       = 1.25f;
                options.taaDepthRejectThreshold = 0.05f;
                options.taaSharpen              = 0.0f;
                break;
            case TaaQuality::High:
                options.taaQualityLevel         = 2;
                options.taaCurrentWeight        = 0.12f;
                options.taaHaltonPeriod         = 16;
                options.taaVarianceGammaY       = 1.35f;
                options.taaVarianceGammaC       = 1.5f;
                options.taaDepthRejectThreshold = 0.03f;
                options.taaSharpen              = 0.15f;
                break;
            case TaaQuality::Ultra:
                options.taaQualityLevel         = 3;
                options.taaCurrentWeight        = 0.10f;
                options.taaHaltonPeriod         = 32;
                options.taaVarianceGammaY       = 1.25f;
                options.taaVarianceGammaC       = 1.4f;
                options.taaDepthRejectThreshold = 0.03f;
                options.taaSharpen              = 0.25f;
                break;
            case TaaQuality::Off:
                break;
        }
    }

    void ApplyBloomQuality(FreyaOptions& options, BloomQuality quality)
    {
        if (quality == BloomQuality::Off)
        {
            options.enableBloom = false;
            return;
        }

        options.enableBloom = true;
        switch (quality)
        {
            case BloomQuality::Low:
                options.bloomResolutionDivisor = 4;
                options.bloomThreshold         = 1.0f;
                options.bloomExtractScale      = 0.8f;
                options.bloomStrength          = 0.5f;
                break;
            case BloomQuality::Medium:
                options.bloomResolutionDivisor = 2;
                options.bloomThreshold         = 0.75f;
                options.bloomExtractScale      = 1.0f;
                options.bloomStrength          = 0.8f;
                break;
            case BloomQuality::High:
                options.bloomResolutionDivisor = 2;
                options.bloomThreshold         = 0.65f;
                options.bloomExtractScale      = 1.1f;
                options.bloomStrength          = 1.0f;
                break;
            case BloomQuality::Ultra:
                options.bloomResolutionDivisor = 1;
                options.bloomThreshold         = 0.55f;
                options.bloomExtractScale      = 1.2f;
                options.bloomStrength          = 1.2f;
                break;
            case BloomQuality::Off:
                break;
        }
    }

    void ApplyAnimationQuality(FreyaOptions& options, AnimationQuality quality)
    {
        auto setBands = [&](const float e0, const float n0, const float e1,
                            const float n1, const float e2, const float n2) {
            options.animLodExitDist[0]  = e0;
            options.animLodEnterDist[0] = n0;
            options.animLodExitDist[1]  = e1;
            options.animLodEnterDist[1] = n1;
            options.animLodExitDist[2]  = e2;
            options.animLodEnterDist[2] = n2;
        };
        auto setHz = [&](const float h0, const float h1, const float h2,
                         const float h3) {
            options.animLodHz[0] = std::max(1.f, h0);
            options.animLodHz[1] = std::max(1.f, h1);
            options.animLodHz[2] = std::max(1.f, h2);
            options.animLodHz[3] = std::max(1.f, h3);
        };

        switch (quality)
        {
            case AnimationQuality::Off:
            case AnimationQuality::Ultra:
                options.enableAnimLod = false;
                setHz(1000.f, 1000.f, 1000.f, 1000.f);
                setBands(1e6f, 1e6f, 1e6f, 1e6f, 1e6f, 1e6f);
                options.animBakeHz = 30.f;
                break;
            case AnimationQuality::Low:
                options.enableAnimLod = true;
                setHz(30.f, 15.f, 8.f, 4.f);
                setBands(8.f, 6.f, 18.f, 14.f, 32.f, 26.f);
                options.animBakeHz = 20.f;
                break;
            case AnimationQuality::Medium:
                options.enableAnimLod = true;
                setHz(45.f, 22.f, 12.f, 6.f);
                setBands(12.f, 10.f, 24.f, 20.f, 42.f, 36.f);
                options.animBakeHz = 30.f;
                break;
            case AnimationQuality::High:
                options.enableAnimLod = true;
                setHz(60.f, 30.f, 15.f, 8.f);
                setBands(20.f, 17.f, 38.f, 32.f, 55.f, 48.f);
                options.animBakeHz = 30.f;
                break;
        }
    }

    [[nodiscard]] float AnimLodHz(const FreyaOptions& o,
                                  const std::uint8_t  tier)
    {
        if (!o.enableAnimLod)
            return 1e6f;
        const auto i = std::min<std::uint8_t>(tier, 3u);
        return std::max(1.f, o.animLodHz[i]);
    }

    [[nodiscard]] float AnimLodMinHz(const FreyaOptions& o)
    {
        if (!o.enableAnimLod)
            return 1e6f;
        return std::min(std::min(std::max(1.f, o.animLodHz[0]),
                                 std::max(1.f, o.animLodHz[1])),
                        std::min(std::max(1.f, o.animLodHz[2]),
                                 std::max(1.f, o.animLodHz[3])));
    }

    /**
     * @brief Advance a per-actor LOD accumulator; true when a pose update
     * is due this display frame. Rate is wall-clock Hz (capped by FPS).
     */
    bool ConsumeAnimLodTick(float& accum, float dt, float hz)
    {
        if (hz >= 1e5f)
        {
            accum = 0.f;
            return true;
        }
        const float interval = 1.f / std::max(hz, 1.f);
        accum += dt;
        if (accum < interval)
            return false;
        accum -= interval;
        if (accum >= interval)
            accum = std::fmod(accum, interval);
        return true;
    }

    /**
     * @brief Hysteresis tier update (0 Near … 3 Far) from camera distance.
     */
    void UpdateAnimLodTier(const FreyaOptions& o, std::uint8_t& tier,
                           float dist)
    {
        if (!o.enableAnimLod)
        {
            tier = 0;
            return;
        }

        switch (tier)
        {
            case 0:
                if (dist > o.animLodExitDist[0])
                    tier = 1;
                break;
            case 1:
                if (dist < o.animLodEnterDist[0])
                    tier = 0;
                else if (dist > o.animLodExitDist[1])
                    tier = 2;
                break;
            case 2:
                if (dist < o.animLodEnterDist[1])
                    tier = 1;
                else if (dist > o.animLodExitDist[2])
                    tier = 3;
                break;
            default:
                if (dist < o.animLodEnterDist[2])
                    tier = 2;
                break;
        }
    }

} // namespace FREYA_NAMESPACE