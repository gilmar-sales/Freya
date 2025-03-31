#pragma once

#include "Freya/Core/ForwardPass.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    class PhysicalDevice;
    class Device;
    class Surface;

    class ForwardPassBuilder
    {
      public:
        ForwardPassBuilder(const Ref<Device>&         device,
                           const Ref<PhysicalDevice>& physicalDevice,
                           const Ref<Surface>&        surface,
                           const Ref<FreyaOptions>&   freyaOptions,
                           const Ref<skr::Logger<ForwardPassBuilder>>& logger,
                           const Ref<skr::ServiceProvider>& serviceProvider) :
            mDevice(device), mPhysicalDevice(physicalDevice), mSurface(surface),
            mFreyaOptions(freyaOptions), mLogger(logger),
            mServiceProvider(serviceProvider)
        {
        }

        Ref<ForwardPass> Build();

      private:
        vk::RenderPass                               createRenderPass() const;
        std::tuple<vk::PipelineLayout, vk::Pipeline> createPipeline() const;

        Ref<skr::Logger<ForwardPassBuilder>> mLogger;
        Ref<Device>                          mDevice;
        Ref<PhysicalDevice>                  mPhysicalDevice;
        Ref<Surface>                         mSurface;
        Ref<skr::ServiceProvider>            mServiceProvider;
        Ref<FreyaOptions>                    mFreyaOptions;
    };
} // namespace FREYA_NAMESPACE
