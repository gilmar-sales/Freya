#pragma once

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
        float             drawDistance = 1000.0f; ///< View distance for culling
        RenderingStrategy renderingStrategy =
            RenderingStrategy::Forward; ///< Rendering strategy
    };

} // namespace FREYA_NAMESPACE
