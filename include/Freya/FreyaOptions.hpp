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
        Ultra,  ///< 4096², full-res locals/mask, every-frame update, 16 taps
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
     * @brief Deferred lighting debug visualization (G-buffer / SSAO / shadows).
     *
     * None keeps the lit path. Any other value replaces lighting output with
     * a debug visualization and disables ACES tonemap in composite.
     * Values match the lighting.frag push-constant debugMode.
     */
    enum class DeferredDebugView : std::uint32_t
    {
        None        = 0,
        Albedo      = 1,
        Normal      = 2,
        Depth       = 3,
        Roughness   = 4,
        Metalness   = 5,
        MaterialAO  = 6,
        MaterialId  = 7,
        Velocity    = 8,
        SsaoBlurred = 9,
        SsaoRaw     = 10,
        Shadows     = 11,
    };

    [[nodiscard]] inline bool IsDeferredDebugActive(
        const DeferredDebugView view)
    {
        return view != DeferredDebugView::None;
    }

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

    /// Absolute side length wins when >0; otherwise cascadeRes / divisor.
    inline std::uint32_t ResolveShadowSideResolution(
        const std::uint32_t cascadeResolution, const std::uint32_t absolute,
        const std::uint32_t divisor)
    {
        if (absolute > 0)
            return absolute;
        return std::max(1u, cascadeResolution / std::max(1u, divisor));
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
        std::uint32_t frameCount   = 3;
        glm::vec4     clearColor   = { 0.0f, 0.0f, 0.0f, 0.0f };
        float         drawDistance = 1000.0f;
        std::uint32_t maxLights    = 64;
        float         iblIntensity = 0.7f;
        float         exposure     = 0.7f;
        std::string   environmentMapPath =
            "./Resources/Environments/studio_small_09_4k.hdr";
        glm::vec3 ambientColor     = glm::vec3(1.0f);
        float     ambientIntensity = 0.03f;

        // --- Shadow maps (prefer SetShadowQuality presets) ---
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
        /// Fraction of cascade split used for blend into next (0 = off).
        float shadowCascadeBlend = 0.0f;
        /// Max view-space distance covered by directional CSM (not draw
        /// distance). Larger values enlarge near cascades and inflate
        /// world-space bias until contact shadows disappear.
        float shadowCascadeDistance = 80.0f;
        /// Half-res directional CSM mask before fullscreen lighting.
        /// Off by default: lighting samples cascades directly (same PCF as
        /// spot/point). The mask looked softer and could lag with camera
        /// motion.
        bool          enableShadowMask            = false;
        std::uint32_t shadowMaskResolutionDivisor = 2;
        /// Kept for API compat; directional CSM may skip redraw when the
        /// camera/sun are stable, but only while reusing the committed VPs
        /// that match the depth maps (1 = every frame).
        std::uint32_t shadowCascadeUpdatePeriod = 2;
        /// Absolute point cube face size; 0 = cascade res / divisor.
        std::uint32_t shadowPointResolution        = 0;
        std::uint32_t shadowPointResolutionDivisor = 2;
        /// Absolute spot map size; 0 = cascade res / divisor.
        std::uint32_t shadowSpotResolution        = 0;
        std::uint32_t shadowSpotResolutionDivisor = 2;
        /// When >1, rebuild a stable point cube every N frames.
        std::uint32_t shadowPointUpdatePeriod = 2;
        bool          ReverseZ;

        std::string shaderRoot = "./Resources/Shaders";

        bool enableShadows = true;
        bool enableSsao    = true;
        bool enableTaa     = true;
        bool enableBloom   = true;

        /// Screen-space diameter (px) at which cull keeps LOD0. Lower =
        /// switch to coarser LODs sooner.
        float meshLodPixelRef = 128.0f;
        /// Diameter shrink factor per LOD step (must be > 1).
        float meshLodStep = 1.75f;

        /// 1 = full, 2 = half, 4 = quarter of render extent.
        std::uint32_t ssaoResolutionDivisor = 2;
        /// Hemisphere radius in view-space meters (LearnOpenGL SSAO).
        /// Human-scale creases ≈ 0.3–1.0.
        float ssaoRadius = 0.5f;
        /// View-Z acne bias (LearnOpenGL default 0.025).
        float             ssaoBias          = 0.025f;
        float             ssaoPower         = 1.5f;
        float             ssaoIntensity     = 0.5f;
        DeferredDebugView deferredDebugView = DeferredDebugView::None;

        /// Blend weight toward current frame (0–1). Higher = less ghosting.
        float taaCurrentWeight = 0.12f;
        /// Halton jitter sequence length used with TAA.
        std::uint32_t taaHaltonPeriod = 16;
        /// Shader feature tier: 0=Low … 3=Ultra (set by ApplyTaaQuality).
        std::uint32_t taaQualityLevel = 2;
        /// YCoCg / RGB variance AABB scale for luminance (or RGB on Medium).
        float taaVarianceGammaY = 1.35f;
        /// YCoCg variance AABB scale for chroma (Co/Cg); High/Ultra only.
        float taaVarianceGammaC = 1.5f;
        /// Soft-reject history when |currDepth - histDepth| exceeds this.
        float taaDepthRejectThreshold = 0.03f;
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
        /// RendererAdvanced::GpuAnimation().RebuildPass()).
        bool quantizeGpuAnimJoints = true;
    };

    void ApplyShadowQuality(FreyaOptions& options, ShadowQuality quality);

    void ApplySsaoQuality(FreyaOptions& options, SsaoQuality quality);

    void ApplyTaaQuality(FreyaOptions& options, TaaQuality quality);

    void ApplyBloomQuality(FreyaOptions& options, BloomQuality quality);

    void ApplyAnimationQuality(FreyaOptions& options, AnimationQuality quality);

    [[nodiscard]] float AnimLodHz(const FreyaOptions& o, std::uint8_t tier);

    [[nodiscard]] float AnimLodMinHz(const FreyaOptions& o);

    /**
     * @brief Advance a per-actor LOD accumulator; true when a pose update
     * is due this display frame. Rate is wall-clock Hz (capped by FPS).
     */
    bool ConsumeAnimLodTick(float& accum, float dt, float hz);

    /**
     * @brief Hysteresis tier update (0 Near … 3 Far) from camera distance.
     */
    void UpdateAnimLodTier(const FreyaOptions& o, std::uint8_t& tier,
                           float dist);
} // namespace FREYA_NAMESPACE
