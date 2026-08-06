#include "RendererBuilder.hpp"

#include "Freya/Builders/BloomPassBuilder.hpp"
#include "Freya/Builders/CommandPoolBuilder.hpp"
#include "Freya/Builders/CompositePassBuilder.hpp"
#include "Freya/Builders/DeferredCompressedPassBuilder.hpp"
#include "Freya/Builders/DeviceBuilder.hpp"
#include "Freya/Builders/ImageBuilder.hpp"
#include "Freya/Builders/PhysicalDeviceBuilder.hpp"
#include "Freya/Builders/RenderPassBuilder.hpp"
#include "Freya/Builders/SurfaceBuilder.hpp"
#include "Freya/Builders/TaaPassBuilder.hpp"
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
        const skr::Arc<RenderPass>&           renderPass,
        const skr::Arc<EventManager>&         eventManager,
        const skr::Arc<Window>&               window,
        const skr::Arc<FreyaOptions>&         freyaOptions,
        const skr::Arc<skr::ServiceProvider>& serviceProvider) :
        mInstance(instance), mSurface(surface), mPhysicalDevice(physicalDevice),
        mDevice(device), mCommandPool(commandPool), mSwapChain(swapChain),
        mRenderPass(renderPass), mEventManager(eventManager), mWindow(window),
        mFreyaOptions(freyaOptions), mServiceProvider(serviceProvider),
        mLogger(serviceProvider->GetService<skr::Logger<RendererBuilder>>())
    {
    }

    skr::Arc<Renderer> RendererBuilder::Build()
    {
        mLogger->LogTrace("Creating renderer - Frame count: {} - Samples: {}",
                          mFreyaOptions->frameCount,
                          static_cast<int>(mFreyaOptions->sampleCount));

        mLogger->LogTrace(
            "Rendering strategy: {}",
            mFreyaOptions->renderingStrategy == RenderingStrategy::Deferred
                ? "Deferred"
                : "Forward");

        skr::Arc<DeferredCompressedPass> deferredPass;
        skr::Arc<BloomPass>              bloomPass;
        skr::Arc<TaaPass>                taaPass;
        skr::Arc<CompositePass>          compositePass;
        skr::Arc<Image>                  forwardColorImage;
        skr::Arc<Image>                  bloomInputImage;

        if (mFreyaOptions->renderingStrategy == RenderingStrategy::Deferred)
        {
            deferredPass =
                mServiceProvider->GetService<DeferredCompressedPassBuilder>()
                    ->Build(mSwapChain);

            // Bloom extracts from pre-TAA Scene Color; composite uses TAA out.
            bloomPass = mServiceProvider->GetService<BloomPassBuilder>()->Build(
                mSwapChain,
                deferredPass->GetSceneColorImage());

            taaPass = mServiceProvider->GetService<TaaPassBuilder>()->Build(
                mSwapChain);

            compositePass =
                mServiceProvider->GetService<CompositePassBuilder>()->Build(
                    mSwapChain);
        }
        else
        {
            // Bloom+Composite also for forward mode.
            // Create forward offscreen images matching the forward pass's
            // MSAA settings so the same pipeline can be reused.
            const auto extent    = mSwapChain->GetExtent();
            const auto format    = mSurface->QuerySurfaceFormat().format;
            const auto depthFmt  = mPhysicalDevice->GetDepthFormat();
            const auto vkSamples = static_cast<vk::SampleCountFlagBits>(
                mFreyaOptions->sampleCount);
            const bool msaa = vkSamples != vk::SampleCountFlagBits::e1;

            // Color attachment (MSAA if requested, matches forward pipeline)
            forwardColorImage =
                mServiceProvider->GetService<ImageBuilder>()
                    ->SetUsage(ImageUsage::Color)
                    .SetFormat(format)
                    .SetWidth(extent.width)
                    .SetHeight(extent.height)
                    .SetSamples(vkSamples)
                    .Build();

            // Resolve / bloom input image (always single sample)
            if (msaa)
            {
                bloomInputImage =
                    mServiceProvider->GetService<ImageBuilder>()
                        ->SetUsage(ImageUsage::Color)
                        .SetFormat(format)
                        .SetWidth(extent.width)
                        .SetHeight(extent.height)
                        .SetSamples(vk::SampleCountFlagBits::e1)
                        .Build();
            }
            else
            {
                bloomInputImage = forwardColorImage;
            }
            // Store bloom input in forwardResolveImage slot via constructor

            // Bloom pass reads the resolve / single-sample color image.
            // Forward depth image is created in
            // Renderer::createForwardOffscreenResources().
            bloomPass = mServiceProvider->GetService<BloomPassBuilder>()->Build(
                mSwapChain,
                bloomInputImage);

            compositePass =
                mServiceProvider->GetService<CompositePassBuilder>()->Build(
                    mSwapChain);
        }

        auto lightService = mServiceProvider->GetService<LightService>();
        auto shadowPass   = mServiceProvider->GetService<ShadowPass>();
        auto pickPass     = mServiceProvider->GetService<PickPass>();

        return skr::MakeArc<Renderer>(
            mInstance,
            mSurface,
            mPhysicalDevice,
            mDevice,
            mSwapChain,
            mRenderPass,
            deferredPass,
            bloomPass,
            taaPass,
            compositePass,
            mCommandPool,
            lightService,
            shadowPass,
            pickPass,
            mServiceProvider,
            mFreyaOptions,
            mEventManager,
            forwardColorImage,
            bloomInputImage);
    }

} // namespace FREYA_NAMESPACE
