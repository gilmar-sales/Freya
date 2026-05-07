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

        Ref<DeferredCompressedPass> deferredPass;
        Ref<BloomPass>              bloomPass;
        Ref<CompositePass>          compositePass;
        Ref<Image>                  forwardColorImage;
        Ref<Image>                  bloomInputImage;

        if (mFreyaOptions->renderingStrategy == RenderingStrategy::Deferred)
        {
            deferredPass =
                mServiceProvider->GetService<DeferredCompressedPassBuilder>()
                    ->Build(mSwapChain);

            // Bloom pass reads the emissive from deferred pass
            bloomPass = mServiceProvider->GetService<BloomPassBuilder>()->Build(
                mSwapChain,
                deferredPass->GetEmissiveImage());

            // Composite pass reads opaque, translucent from deferred pass
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

        return skr::MakeRef<Renderer>(
            mInstance,
            mSurface,
            mPhysicalDevice,
            mDevice,
            mSwapChain,
            mRenderPass,
            deferredPass,
            bloomPass,
            compositePass,
            mCommandPool,
            lightService,
            mServiceProvider,
            mFreyaOptions,
            mEventManager,
            forwardColorImage,
            bloomInputImage);
    }

} // namespace FREYA_NAMESPACE
