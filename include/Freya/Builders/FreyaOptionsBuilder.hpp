#pragma once

#include "Freya/FreyaOptions.hpp"

#include <algorithm>
#include <cstdint>

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
        FreyaOptionsBuilder() : mFreyaOptions(skr::MakeArc<FreyaOptions>()) {};
        ~FreyaOptionsBuilder() = default;

        /**
         * @brief Sets window title.
         * @param title Window title string
         * @return Reference to this for chaining
         */
        FreyaOptionsBuilder& SetTitle(const std::string& title)
        {
            mFreyaOptions->title = title;
            return *this;
        }

        /**
         * @brief Sets window width.
         * @param width Width in pixels
         * @return Reference to this for chaining
         */
        FreyaOptionsBuilder& SetWidth(std::uint32_t width)
        {
            mFreyaOptions->width = width;
            return *this;
        }

        /**
         * @brief Sets window height.
         * @param height Height in pixels
         * @return Reference to this for chaining
         */
        FreyaOptionsBuilder& SetHeight(std::uint32_t height)
        {
            mFreyaOptions->height = height;
            return *this;
        }

        /**
         * @brief Sets vertical synchronization.
         * @param vSync true to enable vsync
         * @return Reference to this for chaining
         */
        FreyaOptionsBuilder& SetVSync(bool vSync)
        {
            mFreyaOptions->vSync = vSync;
            return *this;
        }

        /**
         * @brief Sets fullscreen mode.
         * @param fullscreen true for fullscreen
         * @return Reference to this for chaining
         */
        FreyaOptionsBuilder& SetFullscreen(bool fullscreen)
        {
            mFreyaOptions->fullscreen = fullscreen;
            return *this;
        }

        /**
         * @brief Sets MSAA sample count.
         * @param sampleCount Sample count (1, 2, 4, 8, 16, 32, 64)
         * @return Reference to this for chaining
         */
        FreyaOptionsBuilder& SetSampleCount(std::uint32_t sampleCount)
        {
            mFreyaOptions->sampleCount = sampleCount;
            return *this;
        }

        /**
         * @brief Sets frame count (swapchain image count).
         * @param frameCount Number of frames
         * @return Reference to this for chaining
         */
        FreyaOptionsBuilder& SetFrameCount(std::uint32_t frameCount)
        {
            mFreyaOptions->frameCount = frameCount;
            return *this;
        }

        /**
         * @brief Sets clear color for render pass.
         * @param clearColor Clear color value
         * @return Reference to this for chaining
         */
        FreyaOptionsBuilder& SetClearColor(
            const vk::ClearColorValue& clearColor)
        {
            mFreyaOptions->clearColor = clearColor;
            return *this;
        }

        /**
         * @brief Sets draw distance for frustum culling.
         * @param drawDistance Draw distance in world units
         * @return Reference to this for chaining
         */
        FreyaOptionsBuilder& SetDrawDistance(float drawDistance)
        {
            mFreyaOptions->drawDistance = drawDistance;
            return *this;
        }

        /**
         * @brief Sets maximum number of lights.
         * @param maxLights Maximum light count (default 16)
         * @return Reference to this for chaining
         */
        FreyaOptionsBuilder& SetMaxLights(std::uint32_t maxLights)
        {
            mFreyaOptions->maxLights = maxLights;
            return *this;
        }

        FreyaOptionsBuilder& SetIblIntensity(float intensity)
        {
            mFreyaOptions->iblIntensity = intensity;
            return *this;
        }

        FreyaOptionsBuilder& SetExposure(float exposure)
        {
            mFreyaOptions->exposure = exposure;
            return *this;
        }

        FreyaOptionsBuilder& SetAmbient(const glm::vec3& color, float intensity)
        {
            mFreyaOptions->ambientColor     = color;
            mFreyaOptions->ambientIntensity = intensity;
            return *this;
        }

        FreyaOptionsBuilder& SetEnvironmentMapPath(const std::string& path)
        {
            mFreyaOptions->environmentMapPath = path;
            return *this;
        }

        FreyaOptionsBuilder& SetShadowCascadeCount(std::uint32_t count)
        {
            mFreyaOptions->shadowCascadeCount = count;
            return *this;
        }

        FreyaOptionsBuilder& SetShadowMapResolution(std::uint32_t resolution)
        {
            mFreyaOptions->shadowMapResolution = resolution;
            return *this;
        }

        FreyaOptionsBuilder& SetShadowBias(float bias)
        {
            mFreyaOptions->shadowBias = bias;
            return *this;
        }

        FreyaOptionsBuilder& SetShadowLightSize(float lightSize)
        {
            mFreyaOptions->shadowLightSize = lightSize;
            return *this;
        }

        FreyaOptionsBuilder& SetShadowMaxSoftness(float maxSoftness)
        {
            mFreyaOptions->shadowMaxSoftness = maxSoftness;
            return *this;
        }

        FreyaOptionsBuilder& SetShadowMinVisibility(float minVisibility)
        {
            mFreyaOptions->shadowMinVisibility = minVisibility;
            return *this;
        }

        FreyaOptionsBuilder& SetMaxSpotShadows(std::uint32_t count)
        {
            mFreyaOptions->maxSpotShadows = count;
            return *this;
        }

        FreyaOptionsBuilder& SetMaxPointShadows(std::uint32_t count)
        {
            mFreyaOptions->maxPointShadows = count;
            return *this;
        }

        FreyaOptionsBuilder& SetShadowSampleCount(std::uint32_t count)
        {
            mFreyaOptions->shadowSampleCount = count;
            return *this;
        }

        /**
         * @brief Applies a ShadowQuality preset (resolution, cascades,
         * spot/point slots, soft-shadow tap count). Bias / light size /
         * softness knobs are left unchanged.
         */
        FreyaOptionsBuilder& SetShadowQuality(ShadowQuality quality)
        {
            ApplyShadowQuality(*mFreyaOptions, quality);
            return *this;
        }

        FreyaOptionsBuilder& SetSsaoQuality(SsaoQuality quality)
        {
            ApplySsaoQuality(*mFreyaOptions, quality);
            return *this;
        }

        FreyaOptionsBuilder& SetTaaQuality(TaaQuality quality)
        {
            ApplyTaaQuality(*mFreyaOptions, quality);
            return *this;
        }

        FreyaOptionsBuilder& SetBloomQuality(BloomQuality quality)
        {
            ApplyBloomQuality(*mFreyaOptions, quality);
            return *this;
        }

        /**
         * @brief Applies an AnimationQuality preset (LOD distances /
         * Hz rates / bakeHz). Per-field setters still override after.
         */
        FreyaOptionsBuilder& SetAnimationQuality(AnimationQuality quality)
        {
            ApplyAnimationQuality(*mFreyaOptions, quality);
            return *this;
        }

        FreyaOptionsBuilder& SetAnimLodEnabled(bool enabled)
        {
            mFreyaOptions->enableAnimLod = enabled;
            return *this;
        }

        FreyaOptionsBuilder& SetAnimBakeHz(float hz)
        {
            mFreyaOptions->animBakeHz = std::max(1.f, hz);
            return *this;
        }

        FreyaOptionsBuilder& SetQuantizeGpuAnimJoints(bool enabled)
        {
            mFreyaOptions->quantizeGpuAnimJoints = enabled;
            return *this;
        }

        FreyaOptionsBuilder& SetSsaoResolutionDivisor(std::uint32_t divisor)
        {
            mFreyaOptions->ssaoResolutionDivisor = std::max(1u, divisor);
            return *this;
        }

        FreyaOptionsBuilder& SetSsaoRadius(float radius)
        {
            mFreyaOptions->ssaoRadius = radius;
            return *this;
        }

        FreyaOptionsBuilder& SetSsaoBias(float bias)
        {
            mFreyaOptions->ssaoBias = bias;
            return *this;
        }

        FreyaOptionsBuilder& SetSsaoPower(float power)
        {
            mFreyaOptions->ssaoPower = power;
            return *this;
        }

        FreyaOptionsBuilder& SetSsaoDebugView(SsaoDebugView view)
        {
            mFreyaOptions->ssaoDebugView = view;
            return *this;
        }

        FreyaOptionsBuilder& SetSsaoIntensity(float intensity)
        {
            mFreyaOptions->ssaoIntensity = intensity;
            return *this;
        }

        FreyaOptionsBuilder& SetTaaCurrentWeight(float weight)
        {
            mFreyaOptions->taaCurrentWeight = weight;
            return *this;
        }

        FreyaOptionsBuilder& SetTaaHaltonPeriod(std::uint32_t period)
        {
            mFreyaOptions->taaHaltonPeriod = std::max(1u, period);
            return *this;
        }

        FreyaOptionsBuilder& SetTaaVarianceGammaY(float gamma)
        {
            mFreyaOptions->taaVarianceGammaY = gamma;
            return *this;
        }

        FreyaOptionsBuilder& SetTaaVarianceGammaC(float gamma)
        {
            mFreyaOptions->taaVarianceGammaC = gamma;
            return *this;
        }

        FreyaOptionsBuilder& SetTaaDepthRejectThreshold(float threshold)
        {
            mFreyaOptions->taaDepthRejectThreshold = threshold;
            return *this;
        }

        FreyaOptionsBuilder& SetTaaSharpen(float sharpen)
        {
            mFreyaOptions->taaSharpen = sharpen;
            return *this;
        }

        FreyaOptionsBuilder& SetBloomResolutionDivisor(std::uint32_t divisor)
        {
            mFreyaOptions->bloomResolutionDivisor = std::max(1u, divisor);
            return *this;
        }

        FreyaOptionsBuilder& SetBloomThreshold(float threshold)
        {
            mFreyaOptions->bloomThreshold = threshold;
            return *this;
        }

        FreyaOptionsBuilder& SetBloomExtractScale(float scale)
        {
            mFreyaOptions->bloomExtractScale = scale;
            return *this;
        }

        FreyaOptionsBuilder& SetBloomStrength(float strength)
        {
            mFreyaOptions->bloomStrength = strength;
            return *this;
        }

        FreyaOptionsBuilder& WithReverseZ(bool value = true)
        {
            mFreyaOptions->ReverseZ = value;

            return *this;
        }

        FreyaOptionsBuilder& SetShaderRoot(const std::string& shaderRoot)
        {
            mFreyaOptions->shaderRoot = shaderRoot;
            return *this;
        }

        FreyaOptionsBuilder& SetEnableShadows(bool enable)
        {
            mFreyaOptions->enableShadows = enable;
            return *this;
        }

        FreyaOptionsBuilder& SetEnableSsao(bool enable)
        {
            mFreyaOptions->enableSsao = enable;
            return *this;
        }

        FreyaOptionsBuilder& SetEnableTaa(bool enable)
        {
            mFreyaOptions->enableTaa = enable;
            return *this;
        }

        FreyaOptionsBuilder& SetEnableBloom(bool enable)
        {
            mFreyaOptions->enableBloom = enable;
            return *this;
        }

        /**
         * @brief Builds and returns the FreyaOptions object.
         * @return Shared pointer to configured FreyaOptions
         */
        skr::Arc<FreyaOptions> Build() { return mFreyaOptions; }

      private:
        skr::Arc<FreyaOptions>
            mFreyaOptions; ///< FreyaOptions instance being built
    };

} // namespace FREYA_NAMESPACE
