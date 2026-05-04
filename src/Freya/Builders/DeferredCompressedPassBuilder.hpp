#pragma once

#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Builders/ShaderModuleBuilder.hpp"
#include "Freya/Core/DeferredCompressedPass.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/Core/Surface.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    class SwapChain;

    /**
     * @brief Builder for DeferredCompressedPass objects.
     *
     * Creates the full deferred rendering pipeline including:
     * - Vulkan render pass with 7 attachments and 5 subpasses
     * - 5 graphics pipelines (one per subpass)
     * - G-buffer, depth, translucent, and opaque images
     * - Framebuffers for each swapchain image
     * - Descriptor sets for UBO, input attachments, and samplers
     */
    class DeferredCompressedPassBuilder
    {
      public:
        DeferredCompressedPassBuilder(
            const Ref<Device>&               device,
            const Ref<PhysicalDevice>&       physicalDevice,
            const Ref<Surface>&              surface,
            const Ref<FreyaOptions>&         freyaOptions,
            const Ref<skr::ServiceProvider>& serviceProvider) :
            mDevice(device),
            mPhysicalDevice(physicalDevice),
            mSurface(surface),
            mFreyaOptions(freyaOptions),
            mServiceProvider(serviceProvider)
        {
        }

        /**
         * @brief Builds and returns the DeferredCompressedPass object.
         * @param swapChain The current swapchain (used for framebuffer
         * creation)
         * @return Shared pointer to created DeferredCompressedPass
         */
        Ref<DeferredCompressedPass> Build(const Ref<SwapChain>& swapChain);

        /**
         * @brief Creates the Vulkan render pass for deferred rendering.
         * @return Vulkan render pass handle
         */
        vk::RenderPass createRenderPass() const;

      private:
        Ref<Device>               mDevice;
        Ref<PhysicalDevice>       mPhysicalDevice;
        Ref<Surface>              mSurface;
        Ref<FreyaOptions>         mFreyaOptions;
        Ref<skr::ServiceProvider> mServiceProvider;
    };
} // namespace FREYA_NAMESPACE
