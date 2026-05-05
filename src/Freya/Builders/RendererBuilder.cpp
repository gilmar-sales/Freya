#include "RendererBuilder.hpp"

#include "Freya/Builders/CommandPoolBuilder.hpp"
#include "Freya/Builders/DeferredCompressedPassBuilder.hpp"
#include "Freya/Builders/DeviceBuilder.hpp"
#include "Freya/Builders/PhysicalDeviceBuilder.hpp"
#include "Freya/Builders/RenderPassBuilder.hpp"
#include "Freya/Builders/SurfaceBuilder.hpp"
#include "Freya/Builders/SwapChainBuilder.hpp"

#include "Freya/Core/LightService.hpp"

namespace FREYA_NAMESPACE
{
    RendererBuilder::RendererBuilder(
        const Ref<Instance>&             instance,
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
        mInstance(instance), mSurface(surface), mPhysicalDevice(physicalDevice),
        mDevice(device), mCommandPool(commandPool), mSwapChain(swapChain),
        mRenderPass(renderPass), mEventManager(eventManager), mWindow(window),
        mFreyaOptions(freyaOptions), mServiceProvider(serviceProvider),
        mLogger(serviceProvider->GetService<skr::Logger<RendererBuilder>>())
    {
    }

    Ref<Renderer> RendererBuilder::Build()
    {
        mLogger->LogTrace("Creating renderer - Frame count: {} - Samples: {}",
                          mFreyaOptions->frameCount,
                          static_cast<int>(mFreyaOptions->sampleCount));

        mLogger->LogTrace(
            "Rendering strategy: {}",
            mFreyaOptions->renderingStrategy == RenderingStrategy::Deferred
                ? "Deferred"
                : "Forward");

        // For deferred mode, build the DeferredCompressedPass with the
        // SAME SwapChain that the Renderer will use, ensuring framebuffers
        // reference the correct swapchain images.
        Ref<DeferredCompressedPass> deferredPass;
        if (mFreyaOptions->renderingStrategy == RenderingStrategy::Deferred)
        {
            deferredPass =
                mServiceProvider->GetService<DeferredCompressedPassBuilder>()
                    ->Build(mSwapChain);
        }

        // Get light service
        auto lightService = mServiceProvider->GetService<LightService>();

        return skr::MakeRef<Renderer>(
            mInstance,
            mSurface,
            mPhysicalDevice,
            mDevice,
            mSwapChain,
            mRenderPass,
            deferredPass,
            mCommandPool,
            lightService,
            mServiceProvider,
            mFreyaOptions,
            mEventManager);
    }

} // namespace FREYA_NAMESPACE
