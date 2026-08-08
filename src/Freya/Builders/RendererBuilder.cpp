#include "RendererBuilder.hpp"

#include "Freya/Builders/BloomPassBuilder.hpp"
#include "Freya/Builders/CommandPoolBuilder.hpp"
#include "Freya/Builders/CompositePassBuilder.hpp"
#include "Freya/Builders/DeferredCompressedPassBuilder.hpp"
#include "Freya/Builders/DeviceBuilder.hpp"
#include "Freya/Builders/FsrUpscalePassBuilder.hpp"
#include "Freya/Builders/PhysicalDeviceBuilder.hpp"
#include "Freya/Builders/SsaoPassBuilder.hpp"
#include "Freya/Builders/SurfaceBuilder.hpp"
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

        const auto displayExtent = mSwapChain->GetExtent();
        auto       renderExtent  = displayExtent;
        if (mFreyaOptions->enableFsr &&
            mFreyaOptions->fsrQuality != FsrQuality::Off)
        {
            renderExtent =
                FsrUpscalePass::QueryRenderExtent(displayExtent,
                                                  mFreyaOptions->fsrQuality);
        }

        auto deferredPass =
            mServiceProvider->GetService<DeferredCompressedPassBuilder>()
                ->Build(mSwapChain, renderExtent);

        skr::Arc<FsrUpscalePass> fsrPass;
        if (mFreyaOptions->enableFsr)
        {
            fsrPass = mServiceProvider->GetService<FsrUpscalePassBuilder>()
                          ->Build(mSwapChain, renderExtent, displayExtent);
        }

        skr::Arc<BloomPass> bloomPass;
        if (mFreyaOptions->enableBloom)
        {
            auto bloomSource = deferredPass->GetSceneColorImage();
            auto bloomExtent = renderExtent;
            if (fsrPass && fsrPass->Valid())
            {
                bloomSource = fsrPass->GetOutputImage();
                bloomExtent = displayExtent;
            }
            bloomPass = mServiceProvider->GetService<BloomPassBuilder>()->Build(
                mSwapChain,
                bloomSource,
                bloomExtent);
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
            fsrPass,
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
