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
     * @brief FSR 3 Upscaler quality / scale presets (AMD naming).
     */
    enum class FsrQuality
    {
        NativeAA,         ///< 1.0x (temporal AA at display resolution)
        Quality,          ///< 1.5x
        Balanced,         ///< 1.7x
        Performance,      ///< 2.0x
        UltraPerformance, ///< 3.0x
        Off               ///< no FSR / no Halton jitter
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
        float         shadowLightSize     = 0.03f;
        float         shadowMaxSoftness   = 8.0f;
        float         shadowMinVisibility = 0.0f;
        std::uint32_t maxSpotShadows      = 4;
        std::uint32_t maxPointShadows     = 2;
        std::uint32_t shadowSampleCount   = 16;
        bool          ReverseZ;

        std::string shaderRoot = "./Resources/Shaders";

        bool enableShadows = true;
        bool enableSsao    = true;
        bool enableFsr     = true;
        bool enableBloom   = true;

        /// 1 = full, 2 = half, 4 = quarter of render extent.
        std::uint32_t ssaoResolutionDivisor = 2;
        float         ssaoRadius            = 1.25f;
        float         ssaoBias              = 0.04f;
        float         ssaoPower             = 2.0f;
        float         ssaoIntensity         = 1.35f;

        /// Active FSR quality (ignored when enableFsr is false).
        FsrQuality fsrQuality = FsrQuality::Quality;

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
                options.ssaoRadius            = 1.0f;
                options.ssaoBias              = 0.05f;
                options.ssaoPower             = 1.5f;
                options.ssaoIntensity         = 1.0f;
                break;
            case SsaoQuality::Medium:
                options.ssaoResolutionDivisor = 2;
                options.ssaoRadius            = 1.25f;
                options.ssaoBias              = 0.04f;
                options.ssaoPower             = 2.0f;
                options.ssaoIntensity         = 1.35f;
                break;
            case SsaoQuality::High:
                options.ssaoResolutionDivisor = 2;
                options.ssaoRadius            = 1.5f;
                options.ssaoBias              = 0.035f;
                options.ssaoPower             = 2.2f;
                options.ssaoIntensity         = 1.5f;
                break;
            case SsaoQuality::Ultra:
                options.ssaoResolutionDivisor = 1;
                options.ssaoRadius            = 1.75f;
                options.ssaoBias              = 0.03f;
                options.ssaoPower             = 2.5f;
                options.ssaoIntensity         = 1.6f;
                break;
            case SsaoQuality::Off:
                break;
        }
    }

    inline void ApplyFsrQuality(FreyaOptions& options, FsrQuality quality)
    {
        if (quality == FsrQuality::Off)
        {
            options.enableFsr  = false;
            options.fsrQuality = FsrQuality::Off;
            return;
        }

        options.enableFsr  = true;
        options.fsrQuality = quality;
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
