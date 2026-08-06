#pragma once

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Rendering strategy enumeration.
     */
    enum class RenderingStrategy
    {
        Forward, ///< Forward rendering pass
        Deferred ///< Deferred rendering with G-buffer
    };

    /**
     * @brief Global configuration options for Freya engine.
     *
     * @param title             Window title (default "Freya Window")
     * @param width             Window width in pixels (default 800)
     * @param height            Window height in pixels (default 600)
     * @param vSync             Vertical sync enabled (default true)
     * @param fullscreen        Fullscreen mode (default true)
     * @param sampleCount       MSAA sample count (default 1)
     * @param frameCount        Swapchain frame count (default 4)
     * @param clearColor        Render pass clear color (default black
     * transparent)
     * @param drawDistance      View distance for culling (default 1000.0)
     * @param renderingStrategy Forward or Deferred (default Forward)
     */
    struct FreyaOptions
    {
        std::string         title       = "Freya Window"; ///< Window title
        std::uint32_t       width       = 800;  ///< Window width in pixels
        std::uint32_t       height      = 600;  ///< Window height in pixels
        bool                vSync       = true; ///< Vertical sync enabled
        bool                fullscreen  = true; ///< Fullscreen mode
        std::uint32_t       sampleCount = 1;    ///< MSAA sample count
        std::uint32_t       frameCount  = 4;    ///< Swapchain frame count
        vk::ClearColorValue clearColor  = { 0.0f, 0.0f, 0.0f,
                                            0.0f }; ///< Render pass clear color
        float         drawDistance = 1000.0f; ///< View distance for culling
        std::uint32_t maxLights    = 16;      ///< Maximum lights in scene
        float         iblIntensity = 0.7f;    ///< Image-based lighting scale
        float         exposure     = 0.7f;    ///< HDR exposure before tonemap
        /// Radiance `.hdr` path relative to the process working directory.
        /// Empty string forces the procedural sky. Default file is copied next
        /// to examples under Resources/Environments/.
        std::string environmentMapPath =
            "./Resources/Environments/studio_small_09_4k.hdr";
        glm::vec3 ambientColor =
            glm::vec3(1.0f); ///< Flat ambient tint (legacy / fill)
        float ambientIntensity =
            0.03f; ///< Flat ambient intensity (legacy / fill)
        /// Directional CSM cascade count (1–4).
        std::uint32_t shadowCascadeCount = 4;
        /// Resolution of each cascade / spot / cube face (square).
        std::uint32_t shadowMapResolution = 2048;
        /// Depth bias applied when sampling shadows.
        float shadowBias = 0.002f;
        /// PCSS light disk size (larger → softer distant penumbra).
        float shadowLightSize = 0.03f;
        /// PCSS max PCF kernel radius in shadow-map texels.
        float shadowMaxSoftness = 8.0f;
        /// Max concurrent spot lights casting shadows (0–4).
        std::uint32_t maxSpotShadows = 4;
        /// Max concurrent point lights casting shadows (0–2).
        std::uint32_t maxPointShadows = 2;
        /// Bilateral denoise for directional cascade shadows.
        bool              shadowDenoise            = false;
        float             shadowDenoiseRadius      = 4.0f;
        float             shadowDenoiseDepthSigma  = 0.02f;
        float             shadowDenoiseNormalSigma = 32.0f;
        RenderingStrategy renderingStrategy =
            RenderingStrategy::Forward; ///< Rendering strategy
        bool ReverseZ;
    };

} // namespace FREYA_NAMESPACE
