#pragma once

#include "Freya/Core/DeferredCompressedPass.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/Surface.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Builder for DeferredCompressedPass objects.
     *
     * @param device         Device reference
     * @param surface        Surface reference
     * @param freyaOptions    Freya options reference
     * @param serviceProvider Service provider
     */
    class DeferredCompressedPassBuilder
    {
      public:
        DeferredCompressedPassBuilder(
            const Ref<Device>&               device,
            const Ref<Surface>&              surface,
            const Ref<FreyaOptions>&         freyaOptions,
            const Ref<skr::ServiceProvider>& serviceProvider) :
            mDevice(device), mSurface(surface), mFreyaOptions(freyaOptions),
            mServiceProvider(serviceProvider)
        {
        }

        /**
         * @brief Builds and returns the DeferredCompressedPass object.
         * @return Shared pointer to created DeferredCompressedPass
         */
        [[nodiscard]] Ref<DeferredCompressedPass> Build() const;

        /**
         * @brief Creates the Vulkan render pass for deferred rendering.
         * @return Vulkan render pass handle
         */
        vk::RenderPass createRenderPass() const;

      private:
        Ref<Device>       mDevice;       ///< Device reference
        Ref<Surface>      mSurface;      ///< Surface reference
        Ref<FreyaOptions> mFreyaOptions; ///< Freya options reference
        Ref<skr::ServiceProvider>
            mServiceProvider; ///< Service provider reference
    };
} // namespace FREYA_NAMESPACE
