#pragma once

#include "Freya/FreyaOptions.hpp"

#include "Freya/Core/Renderer.hpp"
#include "Freya/Core/Window.hpp"

namespace FREYA_NAMESPACE
{
    class InstanceBuilder;

    class RendererBuilder
    {
      public:
        RendererBuilder(const Ref<Instance>&             instance,
                        const Ref<Surface>&              surface,
                        const Ref<PhysicalDevice>&       physicalDevice,
                        const Ref<Device>&               device,
                        const Ref<CommandPool>&          commandPool,
                        const Ref<SwapChain>&            swapChain,
                        const Ref<RenderPass>&           renderPass,
                        const Ref<EventManager>&         eventManager,
                        const Ref<Window>&               window,
                        const Ref<FreyaOptions>&         freyaOptions,
                        const Ref<skr::ServiceProvider>& serviceProvider) :
            mSurface(surface), mPhysicalDevice(physicalDevice), mDevice(device),
            mCommandPool(commandPool), mSwapChain(swapChain),
            mRenderPass(renderPass), mWindow(window),
            mEventManager(eventManager), mInstance(instance),
            mFreyaOptions(freyaOptions), mServiceProvider(serviceProvider),
            mLogger(serviceProvider->GetService<skr::Logger<RendererBuilder>>())
        {
        }

        Ref<Renderer> Build();

      private:
        friend class ApplicationBuilder;

        Ref<Instance>       mInstance;
        Ref<Surface>        mSurface;
        Ref<PhysicalDevice> mPhysicalDevice;
        Ref<Device>         mDevice;
        Ref<CommandPool>    mCommandPool;
        Ref<SwapChain>      mSwapChain;
        Ref<EventManager>   mEventManager;
        Ref<RenderPass>     mRenderPass;
        Ref<Window>         mWindow;
        Ref<FreyaOptions>   mFreyaOptions;

        Ref<skr::Logger<RendererBuilder>> mLogger;
        Ref<skr::ServiceProvider>         mServiceProvider;
    };

} // namespace FREYA_NAMESPACE
