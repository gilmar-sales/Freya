#pragma once

#include "Freya/Core/DeferredCompressedPass.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/Surface.hpp"

namespace FREYA_NAMESPACE
{

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

        [[nodiscard]] Ref<DeferredCompressedPass> Build() const;
        vk::RenderPass                            createRenderPass() const;

      private:
        Ref<Device>               mDevice;
        Ref<Surface>              mSurface;
        Ref<FreyaOptions>         mFreyaOptions;
        Ref<skr::ServiceProvider> mServiceProvider;
    };
} // namespace FREYA_NAMESPACE
