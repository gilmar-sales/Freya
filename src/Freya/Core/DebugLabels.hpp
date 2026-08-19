#pragma once

#include <array>
#include <string_view>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Named debug-utils region (RenderDoc / Nsight event browser).
     */
    struct DebugRegion
    {
        const char*          name;
        std::array<float, 4> color; ///< RGBA in [0, 1]
    };

    /**
     * @brief Shared debug-label palette and stage/pass regions.
     *
     * One hue family per pass type so the GPU event browser is scannable.
     */
    namespace DebugLabel
    {
        // --- Pass family colors --------------------------------------------
        inline constexpr std::array<float, 4> FrameColor { 0.72f, 0.72f, 0.72f,
                                                           1.0f };
        inline constexpr std::array<float, 4> PickColor { 0.95f, 0.35f, 0.75f,
                                                          1.0f };
        inline constexpr std::array<float, 4> ShadowColor { 0.95f, 0.55f, 0.18f,
                                                            1.0f };
        inline constexpr std::array<float, 4> GeometryColor { 0.30f, 0.55f,
                                                              0.95f, 1.0f };
        inline constexpr std::array<float, 4> SsaoColor { 0.15f, 0.82f, 0.78f,
                                                          1.0f };
        inline constexpr std::array<float, 4> LightingColor { 0.95f, 0.85f,
                                                              0.25f, 1.0f };
        inline constexpr std::array<float, 4> TaaColor { 0.35f, 0.85f, 0.40f,
                                                         1.0f };
        inline constexpr std::array<float, 4> BloomColor { 0.72f, 0.42f, 0.95f,
                                                           1.0f };
        inline constexpr std::array<float, 4> TranslucentColor { 0.45f, 0.70f,
                                                                 0.95f, 1.0f };
        inline constexpr std::array<float, 4> CompositeColor { 0.95f, 0.42f,
                                                               0.42f, 1.0f };
        inline constexpr std::array<float, 4> BillboardColor { 0.95f, 0.65f,
                                                               0.20f, 1.0f };
        inline constexpr std::array<float, 4> DebugDrawColor { 0.20f, 0.95f,
                                                               0.55f, 1.0f };
        inline constexpr std::array<float, 4> GpuAnimColor { 0.55f, 0.35f,
                                                             0.95f, 1.0f };

        // --- Top-level / pass roots ----------------------------------------
        inline constexpr DebugRegion Frame { "Frame", FrameColor };

        inline constexpr DebugRegion Pick { "Pick", PickColor };
        inline constexpr DebugRegion PickCopy { "Pick Copy Pixel", PickColor };

        inline constexpr DebugRegion Shadow { "Shadow", ShadowColor };
        inline constexpr DebugRegion ShadowCascades { "CSM Cascades",
                                                      ShadowColor };
        inline constexpr DebugRegion ShadowSpots { "Spot Shadows",
                                                   ShadowColor };
        inline constexpr DebugRegion ShadowPoints { "Point Shadows",
                                                    ShadowColor };

        inline constexpr DebugRegion DeferredGeometry { "Deferred Geometry",
                                                        GeometryColor };
        inline constexpr DebugRegion DepthPrePass { "Depth Pre-pass",
                                                    GeometryColor };
        inline constexpr DebugRegion GBuffer { "G-Buffer", GeometryColor };

        inline constexpr DebugRegion Ssao { "SSAO", SsaoColor };
        inline constexpr DebugRegion SsaoAoCompute { "SSAO AO Compute",
                                                     SsaoColor };
        inline constexpr DebugRegion SsaoBlurH { "SSAO Blur H", SsaoColor };
        inline constexpr DebugRegion SsaoBlurV { "SSAO Blur V", SsaoColor };

        inline constexpr DebugRegion DeferredLighting { "Deferred Lighting",
                                                        LightingColor };

        inline constexpr DebugRegion Taa { "TAA", TaaColor };

        inline constexpr DebugRegion Bloom { "Bloom", BloomColor };
        inline constexpr DebugRegion BloomThreshold { "Bloom Threshold",
                                                      BloomColor };
        inline constexpr DebugRegion BloomDownsample { "Bloom Downsample",
                                                       BloomColor };
        inline constexpr DebugRegion BloomUpsample { "Bloom Upsample",
                                                     BloomColor };
        inline constexpr DebugRegion BloomBlit { "Bloom Blit", BloomColor };
        inline constexpr DebugRegion BloomClear { "Bloom Clear", BloomColor };

        inline constexpr DebugRegion Translucent { "Translucent WBOIT",
                                                   TranslucentColor };
        inline constexpr DebugRegion BillboardVfx { "Billboard VFX",
                                                    BillboardColor };
        inline constexpr DebugRegion BillboardUi { "Billboard UI",
                                                   BillboardColor };

        inline constexpr DebugRegion Composite { "Composite", CompositeColor };
        inline constexpr DebugRegion DebugDraw { "Debug Draw", DebugDrawColor };
        inline constexpr DebugRegion GpuAnim { "GPU Anim", GpuAnimColor };
        inline constexpr DebugRegion UI { "UI", CompositeColor };

        /**
         * @brief Map an IFrameStage::Name() to a colored stage region.
         */
        [[nodiscard]] inline DebugRegion ForStage(const char* name)
        {
            if (name == nullptr)
                return { "Unknown", FrameColor };

            const auto view = std::string_view(name);
            if (view == "Pick")
                return { "Pick", PickColor };
            if (view == "Shadow")
                return { "Shadow", ShadowColor };
            if (view == "DeferredGeometry")
                return { "Deferred Geometry", GeometryColor };
            if (view == "SsaoLighting")
                return { "SSAO + Lighting", SsaoColor };
            if (view == "Taa")
                return { "TAA", TaaColor };
            if (view == "Translucent")
                return { "Translucent WBOIT", TranslucentColor };
            if (view == "BillboardVfx")
                return { "Billboard VFX", BillboardColor };
            if (view == "BillboardUi")
                return { "Billboard UI", BillboardColor };
            if (view == "Bloom")
                return { "Bloom", BloomColor };
            if (view == "Composite")
                return { "Composite", CompositeColor };
            if (view == "DebugDraw")
                return { "Debug Draw", DebugDrawColor };
            if (view == "GpuAnim")
                return { "GPU Anim", GpuAnimColor };

            return { name, FrameColor };
        }
    } // namespace DebugLabel
} // namespace FREYA_NAMESPACE
