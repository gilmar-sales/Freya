#pragma once

#include "Freya/Config.hpp"

#include <cstdint>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Opaque Vulkan/SDL handles needed to drive a Dear ImGui back-end
     * from an application.
     *
     * Every pointer is a plain `void*` so no Vulkan or SDL type leaks into the
     * public headers. The app casts each member to the real handle it expects
     * (VkInstance, VkPhysicalDevice, VkDevice, VkQueue, VkRenderPass) and the
     * `window` member to `SDL_Window*`, then feeds them into
     * ImGui_ImplVulkan_Init / ImGui_ImplSDL3_InitForVulkan.
     */
    struct ImGuiNativeHandles
    {
        void* instance       = nullptr; ///< VkInstance
        void* physicalDevice = nullptr; ///< VkPhysicalDevice
        void* device         = nullptr; ///< VkDevice
        void* window        = nullptr; ///< SDL_Window* (SDL3 platform back-end)
        void* graphicsQueue = nullptr; ///< VkQueue (graphics)
        void* presentQueue = nullptr; ///< VkQueue (present, may equal graphics)
        void* renderPass   = nullptr; ///< VkRenderPass of the UI pass
        std::uint32_t viewWidth  = 0; ///< swapchain width in pixels
        std::uint32_t viewHeight = 0; ///< swapchain height in pixels
        std::uint32_t minImageCount =
            1; ///< frames in flight / swapchain images
    };

    /**
     * @brief Descriptor of the offscreen composite frame for ImGui::Image().
     *
     * Populated only while an offscreen viewport target is set. The app casts
     * `imageView` to `VkImageView` and `sampler` to `VkSampler` and submits
     * them through a descriptor set of type eCombinedImageSampler.
     */
    struct ImGuiViewportImage
    {
        void* imageView =
            nullptr;             ///< VkImageView of the offscreen target color
        void* sampler = nullptr; ///< VkSampler for UI sampling
        bool  valid   = false;   ///< false when no viewport target is set
    };
} // namespace FREYA_NAMESPACE
