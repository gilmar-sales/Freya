#pragma once

#include "Freya/FreyaOptions.hpp"

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

        FreyaOptionsBuilder& WithReverseZ(bool value = true)
        {
            mFreyaOptions->ReverseZ = value;

            return *this;
        }

        /**
         * @brief Sets the rendering strategy (Forward or Deferred).
         * @param strategy Rendering strategy enum value
         * @return Reference to this for chaining
         */
        FreyaOptionsBuilder& SetRenderingStrategy(RenderingStrategy strategy)
        {
            mFreyaOptions->renderingStrategy = strategy;
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
