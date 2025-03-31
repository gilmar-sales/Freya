#pragma once

#include "Freya/Core/DeferredPass.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/Surface.hpp"

namespace FREYA_NAMESPACE
{

    class DeferredPassBuilder
    {
      public:
        DeferredPassBuilder(const Ref<Device>&               device,
                            const Ref<Surface>&              surface,
                            const Ref<FreyaOptions>&         freyaOptions,
                            const Ref<skr::ServiceProvider>& serviceProvider) :
            mDevice(device), mSurface(surface), mFreyaOptions(freyaOptions),
            mServiceProvider(serviceProvider)
        {
        }

        [[nodiscard]] Ref<DeferredPass> Build() const;

      private:
        vk::RenderPass          createRenderPass() const;
        DeferredPassDescriptors createDescriptors(
            const DeferredPassAttachments& attachments) const;
        DeferredPassPipeline createPipeline(
            vk::RenderPass                 renderPass,
            const DeferredPassDescriptors& descriptors) const;

        Ref<Device>               mDevice;
        Ref<Surface>              mSurface;
        Ref<FreyaOptions>         mFreyaOptions;
        Ref<skr::ServiceProvider> mServiceProvider;
    };
} // namespace FREYA_NAMESPACE
