#pragma once

#include "Freya/FreyaOptions.hpp"

#include <Skirnir/Skirnir.hpp>

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Fluent builder for FreyaOptions configuration.
     *
     * Provides chainable methods for all FreyaOptions fields.
     */
    class FreyaOptionsBuilder
    {
      public:
        /**
         * @brief Constructs builder with default options.
         */
        FreyaOptionsBuilder();
        ~FreyaOptionsBuilder() = default;

        /**
         * @brief Sets window title.
         * @param title Window title string
         * @return Reference to this for chaining
         */
        FreyaOptionsBuilder& SetTitle(const std::string& title);

        /**
         * @brief Sets window width.
         * @param width Width in pixels
         * @return Reference to this for chaining
         */
        FreyaOptionsBuilder& SetWidth(std::uint32_t width);

        /**
         * @brief Sets window height.
         * @param height Height in pixels
         * @return Reference to this for chaining
         */
        FreyaOptionsBuilder& SetHeight(std::uint32_t height);

        /**
         * @brief Sets vertical synchronization.
         * @param vSync true to enable vsync
         * @return Reference to this for chaining
         */
        FreyaOptionsBuilder& SetVSync(bool vSync);

        /**
         * @brief Sets fullscreen mode.
         * @param fullscreen true for fullscreen
         * @return Reference to this for chaining
         */
        FreyaOptionsBuilder& SetFullscreen(bool fullscreen);

        /**
         * @brief Sets MSAA sample count.
         * @param sampleCount Sample count (1, 2, 4, 8, 16, 32, 64)
         * @return Reference to this for chaining
         */
        FreyaOptionsBuilder& SetSampleCount(std::uint32_t sampleCount);

        /**
         * @brief Sets frame count (swapchain image count).
         * @param frameCount Number of frames
         * @return Reference to this for chaining
         */
        FreyaOptionsBuilder& SetFrameCount(std::uint32_t frameCount);

        /**
         * @brief Sets clear color for render pass.
         * @param clearColor Clear color value
         * @return Reference to this for chaining
         */
        FreyaOptionsBuilder& SetClearColor(const glm::vec4& clearColor);

        /**
         * @brief Sets draw distance for frustum culling.
         * @param drawDistance Draw distance in world units
         * @return Reference to this for chaining
         */
        FreyaOptionsBuilder& SetDrawDistance(float drawDistance);

        /**
         * @brief Sets maximum number of lights.
         * @param maxLights Maximum light count (default 64)
         * @return Reference to this for chaining
         */
        FreyaOptionsBuilder& SetMaxLights(std::uint32_t maxLights);

        FreyaOptionsBuilder& SetIblIntensity(float intensity);

        FreyaOptionsBuilder& SetExposure(float exposure);

        FreyaOptionsBuilder& SetAmbient(const glm::vec3& color,
                                        float            intensity);

        FreyaOptionsBuilder& SetEnvironmentMapPath(const std::string& path);

        FreyaOptionsBuilder& SetShadowCascadeCount(std::uint32_t count);

        FreyaOptionsBuilder& SetShadowMapResolution(std::uint32_t resolution);

        FreyaOptionsBuilder& SetShadowBias(float bias);

        /**
         * @brief Max view-space range of directional CSM (meters).
         * Independent of draw distance; keep modest for contact quality.
         */
        FreyaOptionsBuilder& SetShadowCascadeDistance(float distance);

        /**
         * @brief Fraction of each cascade split used to blend into the next
         * (0 = hard seam, 0.3 = smooth 30% overlap). Higher values hide
         * cascade banding at the cost of sampling two cascades per pixel in
         * the blend zone.
         */
        FreyaOptionsBuilder& SetShadowCascadeBlend(float blend);

        FreyaOptionsBuilder& SetShadowLightSize(float lightSize);

        FreyaOptionsBuilder& SetShadowMaxSoftness(float maxSoftness);

        FreyaOptionsBuilder& SetShadowMinVisibility(float minVisibility);

        FreyaOptionsBuilder& SetMaxSpotShadows(std::uint32_t count);

        FreyaOptionsBuilder& SetMaxPointShadows(std::uint32_t count);

        FreyaOptionsBuilder& SetShadowPointResolution(std::uint32_t resolution);

        FreyaOptionsBuilder& SetShadowPointResolutionDivisor(
            std::uint32_t divisor);

        FreyaOptionsBuilder& SetShadowSpotResolution(std::uint32_t resolution);

        FreyaOptionsBuilder& SetShadowSpotResolutionDivisor(
            std::uint32_t divisor);

        FreyaOptionsBuilder& SetShadowPointUpdatePeriod(std::uint32_t period);

        FreyaOptionsBuilder& SetShadowSampleCount(std::uint32_t count);

        /**
         * @brief Applies a ShadowQuality preset (resolution, cascades,
         * spot/point slots, soft-shadow tap count). Bias / light size /
         * softness knobs are left unchanged.
         */
        FreyaOptionsBuilder& SetShadowQuality(ShadowQuality quality);

        FreyaOptionsBuilder& SetSsaoQuality(SsaoQuality quality);

        FreyaOptionsBuilder& SetTaaQuality(TaaQuality quality);

        FreyaOptionsBuilder& SetBloomQuality(BloomQuality quality);

        /**
         * @brief Applies an AnimationQuality preset (LOD distances /
         * Hz rates / bakeHz). Per-field setters still override after.
         */
        FreyaOptionsBuilder& SetAnimationQuality(AnimationQuality quality);

        FreyaOptionsBuilder& SetAnimLodEnabled(bool enabled);

        FreyaOptionsBuilder& SetAnimBakeHz(float hz);

        FreyaOptionsBuilder& SetQuantizeGpuAnimJoints(bool enabled);

        FreyaOptionsBuilder& SetSsaoResolutionDivisor(std::uint32_t divisor);

        FreyaOptionsBuilder& SetSsaoRadius(float radius);

        FreyaOptionsBuilder& SetSsaoBias(float bias);

        FreyaOptionsBuilder& SetSsaoPower(float power);

        FreyaOptionsBuilder& SetDeferredDebugView(DeferredDebugView view);

        FreyaOptionsBuilder& SetSsaoIntensity(float intensity);

        FreyaOptionsBuilder& SetTaaCurrentWeight(float weight);

        FreyaOptionsBuilder& SetTaaHaltonPeriod(std::uint32_t period);

        FreyaOptionsBuilder& SetTaaVarianceGammaY(float gamma);

        FreyaOptionsBuilder& SetTaaVarianceGammaC(float gamma);

        FreyaOptionsBuilder& SetTaaDepthRejectThreshold(float threshold);

        FreyaOptionsBuilder& SetTaaSharpen(float sharpen);

        FreyaOptionsBuilder& SetBloomResolutionDivisor(std::uint32_t divisor);

        FreyaOptionsBuilder& SetBloomThreshold(float threshold);

        FreyaOptionsBuilder& SetBloomExtractScale(float scale);

        FreyaOptionsBuilder& SetBloomStrength(float strength);

        FreyaOptionsBuilder& WithReverseZ(bool value = true);

        FreyaOptionsBuilder& SetShaderRoot(const std::string& shaderRoot);

        FreyaOptionsBuilder& SetEnableShadows(bool enable);

        FreyaOptionsBuilder& SetEnableSsao(bool enable);

        FreyaOptionsBuilder& SetEnableTaa(bool enable);

        FreyaOptionsBuilder& SetEnableBloom(bool enable);

        /// Lower = pick coarser mesh LODs sooner (screen diameter in px).
        FreyaOptionsBuilder& SetMeshLodPixelRef(float pixels);

        /// Diameter shrink per LOD step (> 1). Lower = denser LOD ladder.
        FreyaOptionsBuilder& SetMeshLodStep(float step);

        /**
         * @brief Builds and returns the FreyaOptions object.
         * @return Shared pointer to configured FreyaOptions
         */
        skr::Arc<FreyaOptions> Build();

      private:
        skr::Arc<FreyaOptions>
            mFreyaOptions; ///< FreyaOptions instance being built
    };

} // namespace FREYA_NAMESPACE
