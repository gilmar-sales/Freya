#pragma once

#include "Freya/Core/SwapChain.hpp"

#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    class Instance;
    class PhysicalDevice;
    class Device;
    class Surface;
    class ForwardPass;

    class SwapChainBuilder
    {
      public:
        SwapChainBuilder(const Ref<Instance>&       instance,
                         const Ref<PhysicalDevice>& physicalDevice,
                         const Ref<Device>&         device,
                         const Ref<Surface>&        surface,
                         const Ref<ForwardPass>&    renderPass,
                         const Ref<FreyaOptions>&   freyaOptions,
                         const Ref<skr::Logger<SwapChainBuilder>>& logger,
                         const Ref<skr::ServiceProvider>& serviceProvider) :
            mInstance(instance), mPhysicalDevice(physicalDevice),
            mDevice(device), mSurface(surface), mRenderPass(renderPass),
            mFreyaOptions(freyaOptions), mLogger(logger),
            mServiceProvider(serviceProvider)
        {
        }

        Ref<SwapChain> Build();

      protected:
        vk::PresentModeKHR choosePresentMode();

      private:
        Ref<Instance>       mInstance;
        Ref<PhysicalDevice> mPhysicalDevice;
        Ref<Device>         mDevice;
        Ref<Surface>        mSurface;
        Ref<ForwardPass>    mRenderPass;
        Ref<FreyaOptions>   mFreyaOptions;

        Ref<skr::Logger<SwapChainBuilder>> mLogger;
        Ref<skr::ServiceProvider>          mServiceProvider;
    };

} // namespace FREYA_NAMESPACE
