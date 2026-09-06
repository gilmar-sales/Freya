#pragma once

#include "Freya/Config.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

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
        Low,    ///< half-res, softer / cheaper knobs
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
     * @brief Animation rate LOD preset (crowd / many skinned actors).
     *
     * Controls distance bands and per-tier pose update rates in Hz
     * (wall-clock), independent of display FPS. Playback never freezes:
     * skipped frames keep the last skin palette. Applied via
     * FreyaOptionsBuilder::SetAnimationQuality.
     */
    enum class AnimationQuality
    {
        Low,    ///< short Near band, lower Far Hz
        Medium, ///< balanced 4-tier bands
        High,   ///< wider Near / Mid (default)
        Ultra,  ///< all actors skin every display frame
        Off     ///< disable rate LOD (same as Ultra for cost)
    };

    /**
     * @brief Pixel extent used by FreyaOptions helpers (Vulkan-free).
     */
    struct Extent2D
    {
        std::uint32_t width  = 0;
        std::uint32_t height = 0;
    };

    /**
     * @brief Scales a full render extent by an integer divisor (≥1).
     */
    inline Extent2D ScaledExtent(Extent2D full, std::uint32_t divisor)
    {
        divisor = std::max(1u, divisor);
        return Extent2D { std::max(1u, full.width / divisor),
                          std::max(1u, full.height / divisor) };
    }

    /**
     * @brief Global configuration options for Freya engine.
     */
    struct FreyaOptions
    {
        std::string   title        = "Freya Window";
        std::uint32_t width        = 800;
        std::uint32_t height       = 600;
        bool          vSync        = true;
        bool          fullscreen   = true;
        std::uint32_t sampleCount  = 1;
        std::uint32_t frameCount   = 4;
        glm::vec4     clearColor   = { 0.0f, 0.0f, 0.0f, 0.0f };
        float         drawDistance = 1000.0f;
        std::uint32_t maxLights    = 64;
        float         iblIntensity = 0.7f;
        float         exposure     = 0.7f;
        std::string   environmentMapPath =
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
        bool shadowDebug   = false;
        bool enableSsao    = true;
        bool enableTaa     = true;
        bool enableBloom   = true;

        /// 1 = full, 2 = half, 4 = quarter of render extent.
        std::uint32_t ssaoResolutionDivisor = 2;
        /// Hemisphere radius in view-space meters (LearnOpenGL SSAO).
        /// Human-scale creases ≈ 0.3–1.0.
        float ssaoRadius = 0.5f;
        /// View-Z acne bias (LearnOpenGL default 0.025).
        float         ssaoBias      = 0.025f;
        float         ssaoPower     = 1.5f;
        float         ssaoIntensity = 0.5f;
        SsaoDebugView ssaoDebugView = SsaoDebugView::None;

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

        /// When false, every skinned actor evaluates / skins each frame.
        bool enableAnimLod = true;
        /// Target pose updates/sec for tiers 0..3 (Near→Far). Capped by FPS.
        float animLodHz[4] = { 60.f, 30.f, 15.f, 8.f };
        /// Leave tier i toward i+1 when distance exceeds (metres).
        float animLodExitDist[3] = { 20.f, 38.f, 55.f };
        /// Enter tier i from i+1 when distance falls below (hysteresis).
        float animLodEnterDist[3] = { 17.f, 32.f, 48.f };
        /// Clip bake rate used by apps that call BakeClip with this knob.
        float animBakeHz = 30.f;
        /// When true, GPU clip/rest joints use 16 B quantized storage
        /// (`skin_bake_quant`); otherwise full float TRS (`skin_bake`).
        /// Toggle requires rebuilding GpuAnimPass (see
        /// Renderer::RebuildGpuAnimPass).
        bool quantizeGpuAnimJoints = true;
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

    inline void ApplyAnimationQuality(FreyaOptions&    options,
                                      AnimationQuality quality)
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

    [[nodiscard]] inline float AnimLodHz(const FreyaOptions& o,
                                         const std::uint8_t  tier)
    {
        if (!o.enableAnimLod)
            return 1e6f;
        const auto i = std::min<std::uint8_t>(tier, 3u);
        return std::max(1.f, o.animLodHz[i]);
    }

    [[nodiscard]] inline float AnimLodMinHz(const FreyaOptions& o)
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
    inline bool ConsumeAnimLodTick(float& accum, const float dt, const float hz)
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
    inline void UpdateAnimLodTier(const FreyaOptions& o, std::uint8_t& tier,
                                  const float dist)
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
