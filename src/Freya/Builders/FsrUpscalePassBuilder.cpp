#include "FsrUpscalePassBuilder.hpp"

#include "Freya/Builders/ImageBuilder.hpp"

namespace FREYA_NAMESPACE
{
    FsrUpscalePassBuilder::FsrUpscalePassBuilder(
        const skr::Arc<Device>&               device,
        const skr::Arc<PhysicalDevice>&       physicalDevice,
        const skr::Arc<Surface>&              surface,
        const skr::Arc<FreyaOptions>&         freyaOptions,
        const skr::Arc<skr::ServiceProvider>& serviceProvider) :
        mDevice(device), mPhysicalDevice(physicalDevice), mSurface(surface),
        mFreyaOptions(freyaOptions), mServiceProvider(serviceProvider)
    {
    }

    skr::Arc<FsrUpscalePass> FsrUpscalePassBuilder::Build(
        const skr::Arc<SwapChain>& swapChain, vk::Extent2D renderExtent,
        vk::Extent2D displayExtent)
    {
        if (displayExtent.width == 0 || displayExtent.height == 0)
            displayExtent = swapChain->GetExtent();
        if (renderExtent.width == 0 || renderExtent.height == 0)
            renderExtent = displayExtent;

        auto outputImage =
            mServiceProvider->GetService<ImageBuilder>()
                ->SetUsage(ImageUsage::FsrOutput)
                .SetFormat(vk::Format::eR16G16B16A16Sfloat)
                .SetWidth(displayExtent.width)
                .SetHeight(displayExtent.height)
                .SetSamples(vk::SampleCountFlagBits::e1)
                .Build();

        return skr::MakeArc<FsrUpscalePass>(
            mDevice, mPhysicalDevice, mFreyaOptions, std::move(outputImage),
            renderExtent, displayExtent);
    }
} // namespace FREYA_NAMESPACE
