#include "RendererBuilder.hpp"

#include "Freya/Builders/BloomPassBuilder.hpp"
#include "Freya/Builders/CommandPoolBuilder.hpp"
#include "Freya/Builders/CompositePassBuilder.hpp"
#include "Freya/Builders/DeferredCompressedPassBuilder.hpp"
#include "Freya/Builders/DeviceBuilder.hpp"
#include "Freya/Builders/PhysicalDeviceBuilder.hpp"
#include "Freya/Builders/SsaoPassBuilder.hpp"
#include "Freya/Builders/SurfaceBuilder.hpp"
#include "Freya/Builders/TaaPassBuilder.hpp"
#include "Freya/Builders/XessPassBuilder.hpp"
#include "Freya/Core/LightService.hpp"
#include "Freya/Core/PickPass.hpp"
#include "Freya/Core/ShadowPass.hpp"

namespace FREYA_NAMESPACE
{
    RendererBuilder::RendererBuilder(
        const skr::Arc<Instance>&             instance,
        const skr::Arc<Surface>&              surface,
        const skr::Arc<PhysicalDevice>&       physicalDevice,
        const skr::Arc<Device>&               device,
        const skr::Arc<CommandPool>&          commandPool,
        const skr::Arc<SwapChain>&            swapChain,
        const skr::Arc<EventManager>&         eventManager,
        const skr::Arc<Window>&               window,
        const skr::Arc<FreyaOptions>&         freyaOptions,
        const skr::Arc<skr::ServiceProvider>& serviceProvider) :
        mInstance(instance), mSurface(surface), mPhysicalDevice(physicalDevice),
        mDevice(device), mCommandPool(commandPool), mSwapChain(swapChain),
        mEventManager(eventManager), mWindow(window),
        mFreyaOptions(freyaOptions), mServiceProvider(serviceProvider),
        mLogger(serviceProvider->GetService<skr::Logger<RendererBuilder>>())
    {
    }

    skr::Arc<Renderer> RendererBuilder::Build()
    {
        mLogger->LogTrace("Creating renderer - Frame count: {} - Samples: {}",
                          mFreyaOptions->frameCount,
                          static_cast<int>(mFreyaOptions->sampleCount));

        const auto presentExtent = mSurface->QueryExtent();

        skr::Arc<XessPass> xessPass;
        vk::Extent2D       renderExtent = presentExtent;
        if (mFreyaOptions->enableXess)
        {
            xessPass = mServiceProvider->GetService<XessPassBuilder>()->Build(
                mSwapChain,
                presentExtent);
            if (xessPass)
            {
                renderExtent = xessPass->GetInputExtent();
                xessPass->ResetHistory();
            }
            else
            {
                mLogger->LogWarning(
                    "XeSS init failed; falling back without XeSS.");
                mFreyaOptions->enableXess  = false;
                mFreyaOptions->xessQuality = XessQuality::Off;
            }
        }

        auto deferredPass =
            mServiceProvider->GetService<DeferredCompressedPassBuilder>()
                ->Build(mSwapChain, renderExtent);

        skr::Arc<BloomPass> bloomPass;
        if (mFreyaOptions->enableBloom)
        {
            bloomPass = mServiceProvider->GetService<BloomPassBuilder>()->Build(
                mSwapChain,
                deferredPass->GetSceneColorImage(),
                renderExtent);
        }

        skr::Arc<TaaPass> taaPass;
        if (mFreyaOptions->enableTaa && !mFreyaOptions->enableXess)
        {
            taaPass = mServiceProvider->GetService<TaaPassBuilder>()->Build(
                mSwapChain,
                renderExtent);
        }

        skr::Arc<SsaoPass> ssaoPass;
        if (mFreyaOptions->enableSsao)
        {
            ssaoPass = mServiceProvider->GetService<SsaoPassBuilder>()->Build(
                mSwapChain,
                renderExtent);
        }

        auto compositePass =
            mServiceProvider->GetService<CompositePassBuilder>()->Build(
                mSwapChain);

        auto lightService = mServiceProvider->GetService<LightService>();
        auto shadowPass   = mServiceProvider->GetService<ShadowPass>();
        auto pickPass     = mServiceProvider->GetService<PickPass>();

        return skr::MakeArc<Renderer>(
            mInstance,
            mSurface,
            mPhysicalDevice,
            mDevice,
            mSwapChain,
            deferredPass,
            bloomPass,
            taaPass,
            xessPass,
            ssaoPass,
            compositePass,
            mCommandPool,
            lightService,
            shadowPass,
            pickPass,
            mServiceProvider,
            mFreyaOptions,
            mEventManager);
    }

} // namespace FREYA_NAMESPACE
