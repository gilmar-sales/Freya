#pragma once

#include <algorithm>
#include <cstdint>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Preset budgets for shadow map resolution, cascade / spot /
     * point slots, and soft-shadow Poisson tap count.
     *
     * Applied via FreyaOptionsBuilder::SetShadowQuality. Individual
     * setters still override fields after the preset.
     */
    enum class ShadowQuality
    {
        Low,    ///< 512², 2 cascades, 2 spot, 2 point, 4 taps
        Medium, ///< 1024², 3 cascades, 4 spot, 2 point, 8 taps
        High,   ///< 2048², 4 cascades, 4 spot, 2 point, 16 taps
        Ultra,  ///< 4096², 4 cascades, 4 spot, 2 point, 16 taps
        Off     ///< skip shadow maps; lighting ignores castShadows
    };

    /**
     * @brief SSAO cost / fidelity preset (resolution scale + AO knobs).
     */
    enum class SsaoQuality
    {
        Low,    ///< quarter-res, softer / cheaper
        Medium, ///< half-res (default look)
        High,   ///< half-res, stronger occlusion
        Ultra,  ///< full-res, strongest
        Off     ///< lighting uses white AO fallback
    };

    /**
     * @brief SSAO debug visualization for lighting / composite.
     *
     * None keeps the normal lit path. Blurred / Raw replace lighting with
     * grayscale AO (LDR composite, no ACES) using the post-blur or raw
     * R8 buffer respectively.
     */
    enum class SsaoDebugView
    {
        None,    ///< lit scene with SSAO applied to IBL
        Blurred, ///< grayscale blurred AO
        Raw      ///< grayscale pre-blur AO
    };

    /**
     * @brief TAA responsiveness vs stability preset.
     */
    enum class TaaQuality
    {
        Low,    ///< cross AABB RGB, bilinear, Halton-4
        Medium, ///< 3×3 variance RGB, depth reject, soft sharpen
        High,   ///< YCoCg variance, Catmull-Rom, CAS-like sharpen
        Ultra,  ///< Gaussian YCoCg + directional clip, HQ Catmull-Rom
        Off     ///< no temporal resolve / Halton jitter
    };

    /**
     * @brief Bloom resolution + extract / composite strength preset.
     */
    enum class BloomQuality
    {
        Low,    ///< quarter-res, higher threshold, weaker
        Medium, ///< half-res (default)
        High,   ///< half-res, stronger bloom
        Ultra,  ///< full-res, strongest
        Off     ///< composite bloom tap cleared to black
    };

    /**
     * @brief Scales a full render extent by an integer divisor (≥1).
     */
    inline vk::Extent2D ScaledExtent(vk::Extent2D full, std::uint32_t divisor)
    {
        divisor = std::max(1u, divisor);
        return vk::Extent2D { std::max(1u, full.width / divisor),
                              std::max(1u, full.height / divisor) };
    }

    /**
     * @brief Global configuration options for Freya engine.
     */
    struct FreyaOptions
    {
        std::string         title        = "Freya Window";
        std::uint32_t       width        = 800;
        std::uint32_t       height       = 600;
        bool                vSync        = true;
        bool                fullscreen   = true;
        std::uint32_t       sampleCount  = 1;
        std::uint32_t       frameCount   = 4;
        vk::ClearColorValue clearColor   = { 0.0f, 0.0f, 0.0f, 0.0f };
        float               drawDistance = 1000.0f;
        std::uint32_t       maxLights    = 16;
        float               iblIntensity = 0.7f;
        float               exposure     = 0.7f;
        std::string         environmentMapPath =
            "./Resources/Environments/studio_small_09_4k.hdr";
        glm::vec3 ambientColor     = glm::vec3(1.0f);
        float     ambientIntensity = 0.03f;

        std::uint32_t shadowCascadeCount  = 4;
        std::uint32_t shadowMapResolution = 2048;
        float         shadowBias          = 0.002f;
        /// World-space penumbra radius hint (scaled by cascade texel size).
        float shadowLightSize = 0.03f;
        /// Soft-shadow kernel clamp in shadow-map texels.
        float         shadowMaxSoftness   = 8.0f;
        float         shadowMinVisibility = 0.0f;
        std::uint32_t maxSpotShadows      = 4;
        std::uint32_t maxPointShadows     = 2;
        std::uint32_t shadowSampleCount   = 16;
        bool          ReverseZ;

        std::string shaderRoot = "./Resources/Shaders";

        bool enableShadows = true;
        bool enableSsao    = true;
        bool enableTaa     = true;
        bool enableBloom   = true;

        /// 1 = full, 2 = half, 4 = quarter of render extent.
        std::uint32_t ssaoResolutionDivisor = 2;
        /// View-space meters (human-scale scene ≈ 0.3–1.0).
        float         ssaoRadius            = 0.5f;
        float         ssaoBias              = 0.025f;
        float         ssaoPower             = 1.5f;
        float         ssaoIntensity         = 0.5f;
        SsaoDebugView ssaoDebugView         = SsaoDebugView::None;

        /// Blend weight toward current frame (0–1). Higher = less ghosting.
        float taaCurrentWeight = 0.1f;
        /// Halton jitter sequence length used with TAA.
        std::uint32_t taaHaltonPeriod = 16;
        /// Shader feature tier: 0=Low … 3=Ultra (set by ApplyTaaQuality).
        std::uint32_t taaQualityLevel = 2;
        /// YCoCg / RGB variance AABB scale for luminance (or RGB on Medium).
        float taaVarianceGammaY = 1.35f;
        /// YCoCg variance AABB scale for chroma (Co/Cg); High/Ultra only.
        float taaVarianceGammaC = 1.5f;
        /// Soft-reject history when |currDepth - histDepth| exceeds this.
        float taaDepthRejectThreshold = 0.04f;
        /// Post-resolve sharpen strength (0 = off). High+ only.
        float taaSharpen = 0.15f;

        std::uint32_t bloomResolutionDivisor = 2;
        float         bloomThreshold         = 0.75f;
        float         bloomExtractScale      = 1.0f;
        float         bloomStrength          = 0.8f;
    };

    inline void ApplyShadowQuality(FreyaOptions& options, ShadowQuality quality)
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
                options.shadowMapResolution = 512;
                options.shadowCascadeCount  = 2;
                options.maxSpotShadows      = 2;
                options.maxPointShadows     = 2;
                options.shadowSampleCount   = 4;
                break;
            case ShadowQuality::Medium:
                options.shadowMapResolution = 1024;
                options.shadowCascadeCount  = 3;
                options.maxSpotShadows      = 4;
                options.maxPointShadows     = 2;
                options.shadowSampleCount   = 8;
                break;
            case ShadowQuality::High:
                options.shadowMapResolution = 2048;
                options.shadowCascadeCount  = 4;
                options.maxSpotShadows      = 4;
                options.maxPointShadows     = 2;
                options.shadowSampleCount   = 16;
                break;
            case ShadowQuality::Ultra:
                options.shadowMapResolution = 4096;
                options.shadowCascadeCount  = 4;
                options.maxSpotShadows      = 4;
                options.maxPointShadows     = 2;
                options.shadowSampleCount   = 16;
                break;
            case ShadowQuality::Off:
                break;
        }
    }

    inline void ApplySsaoQuality(FreyaOptions& options, SsaoQuality quality)
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
                options.ssaoResolutionDivisor = 4;
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

    inline void ApplyTaaQuality(FreyaOptions& options, TaaQuality quality)
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
                options.taaCurrentWeight        = 0.1f;
                options.taaHaltonPeriod         = 16;
                options.taaVarianceGammaY       = 1.35f;
                options.taaVarianceGammaC       = 1.5f;
                options.taaDepthRejectThreshold = 0.04f;
                options.taaSharpen              = 0.15f;
                break;
            case TaaQuality::Ultra:
                options.taaQualityLevel         = 3;
                options.taaCurrentWeight        = 0.08f;
                options.taaHaltonPeriod         = 32;
                options.taaVarianceGammaY       = 1.25f;
                options.taaVarianceGammaC       = 1.4f;
                options.taaDepthRejectThreshold = 0.035f;
                options.taaSharpen              = 0.25f;
                break;
            case TaaQuality::Off:
                break;
        }
    }

    inline void ApplyBloomQuality(FreyaOptions& options, BloomQuality quality)
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

} // namespace FREYA_NAMESPACE
