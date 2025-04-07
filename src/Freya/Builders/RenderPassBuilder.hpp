#pragma once

#include "Freya/Core/RenderPass.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    class PhysicalDevice;
    class Device;
    class Surface;

    class RenderPassBuilder
    {
      public:
        RenderPassBuilder(const Ref<Device>&         device,
                          const Ref<PhysicalDevice>& physicalDevice,
                          const Ref<Surface>&        surface,
                          const Ref<FreyaOptions>&   freyaOptions,
                          const Ref<skr::Logger<RenderPassBuilder>>& logger,
                          const Ref<skr::ServiceProvider>& serviceProvider) :
            mDevice(device), mPhysicalDevice(physicalDevice), mSurface(surface),
            mFreyaOptions(freyaOptions), mLogger(logger),
            mServiceProvider(serviceProvider)
        {
        }

        Ref<RenderPass> Build();

      private:
        vk::RenderPass                         createRenderPass() const;
        std::vector<vk::AttachmentDescription> createAttachments() const;
        std::vector<vk::SubpassDependency>     createDependencies() const;

        Ref<skr::Logger<RenderPassBuilder>> mLogger;
        Ref<Device>                         mDevice;
        Ref<PhysicalDevice>                 mPhysicalDevice;
        Ref<Surface>                        mSurface;
        Ref<skr::ServiceProvider>           mServiceProvider;
        Ref<FreyaOptions>                   mFreyaOptions;
    };
} // namespace FREYA_NAMESPACE
