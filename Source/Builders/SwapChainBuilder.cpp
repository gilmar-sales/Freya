#include "Builders/SwapChainBuilder.hpp"

#include "Builders/SurfaceBuilder.hpp"

#include "Core/Device.hpp"
#include "Core/PhysicalDevice.hpp"
#include "Core/RenderPass.hpp"
#include <Builders/ImageBuilder.hpp>

#include <vulkan/vulkan_to_string.hpp>

namespace FREYA_NAMESPACE
{

    std::shared_ptr<SwapChain> SwapChainBuilder::Build()
    {
        auto surfaceFormat = mSurface->QuerySurfaceFormat();
        auto presentMode = choosePresentMode();
        auto extent = mSurface->QueryExtent();

        std::cout << "Frame count: " << mFrameCount << "\n";
        std::cout << "Sample count: " << vk::to_string(mSamples) << "\n";
        std::cout << "Surface format: " << vk::to_string(surfaceFormat.format) << "\n";
        std::cout << "Present Mode:" << vk::to_string(presentMode) << "\n";
        std::cout << "Extent:" << extent.width << ", " << extent.height << "\n";

        auto supportDetails = mPhysicalDevice->QuerySwapChainSupport(mSurface->Get());

        auto createInfo =
            vk::SwapchainCreateInfoKHR()
                .setSurface(mSurface->Get())
                .setImageFormat(surfaceFormat.format)
                .setImageColorSpace(surfaceFormat.colorSpace)
                .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
                .setImageArrayLayers(1)
                .setPresentMode(presentMode)
                .setImageExtent(extent)
                .setMinImageCount(mFrameCount)
                .setPreTransform(supportDetails.capabilities.currentTransform)
                .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
                .setImageSharingMode(vk::SharingMode::eExclusive)
                .setClipped(true);

        assert(mDevice->GetQueueFamilyIndices().isComplete() &&
               "Could not set image sharing mode with incomplete queue families");

        if (mDevice->GetQueueFamilyIndices().isUnique())
        {
            std::uint32_t queueFamilyIndices[] = {
                mDevice->GetQueueFamilyIndices().graphicsFamily.value(),
                mDevice->GetQueueFamilyIndices().presentFamily.value()};

            createInfo.setImageSharingMode(vk::SharingMode::eConcurrent)
                .setQueueFamilyIndices(queueFamilyIndices);
        }

        std::cout << "Sharing mode:" << vk::to_string(createInfo.imageSharingMode)
                  << "\n";

        auto swapChain = mDevice->Get().createSwapchainKHR(createInfo);

        assert(swapChain && "Failed to create swap chain");

        auto swapChainImages = mDevice->Get().getSwapchainImagesKHR(swapChain);

        auto imageViewCreateInfo =
            vk::ImageViewCreateInfo()
                .setViewType(vk::ImageViewType::e2D)
                .setFormat(surfaceFormat.format)
                .setComponents(vk::ComponentMapping()
                                   .setR(vk::ComponentSwizzle::eIdentity)
                                   .setG(vk::ComponentSwizzle::eIdentity)
                                   .setB(vk::ComponentSwizzle::eIdentity)
                                   .setA(vk::ComponentSwizzle::eIdentity))
                .setSubresourceRange(vk::ImageSubresourceRange()
                                         .setAspectMask(vk::ImageAspectFlagBits::eColor)
                                         .setBaseMipLevel(0)
                                         .setLevelCount(1)
                                         .setBaseArrayLayer(0)
                                         .setLayerCount(1));

        auto frames = std::vector<SwapChainFrame>(swapChainImages.size());

        auto depthImage =
            ImageBuilder(mDevice)
                .SetUsage(ImageUsage::Depth)
                .SetSamples(mSamples)
                .SetWidth(extent.width)
                .SetHeight(extent.height)
                .Build();

        auto sampleImage =
            ImageBuilder(mDevice)
                .SetUsage(ImageUsage::Sampling)
                .SetSamples(mSamples)
                .SetWidth(extent.width)
                .SetHeight(extent.height)
                .Build();

        for (auto index = 0; index < swapChainImages.size(); index++)
        {
            frames[index].image = swapChainImages[index];

            imageViewCreateInfo.setImage(swapChainImages[index]);
            frames[index].imageView = mDevice->Get().createImageView(imageViewCreateInfo);
            assert(frames[index].imageView && "Failed to create image views");

            auto attachments =
                mSamples != vk::SampleCountFlagBits::e1
                    ? std::vector<vk::ImageView>{sampleImage->GetImageView(),
                                                 depthImage->GetImageView(),
                                                 frames[index].imageView}
                    : std::vector<vk::ImageView>{
                          frames[index].imageView, depthImage->GetImageView()};

            auto framebufferInfo =
                vk::FramebufferCreateInfo()
                    .setRenderPass(mRenderPass->Get())
                    .setAttachments(attachments)
                    .setWidth(extent.width)
                    .setHeight(extent.height)
                    .setLayers(1);

            frames[index].frameBuffer = mDevice->Get().createFramebuffer(framebufferInfo);

            assert(frames[index].frameBuffer && "Failed to create framebuffer");
        }

        return std::make_shared<SwapChain>(
            mDevice, mInstance, mSurface, swapChain, frames, depthImage, sampleImage);
    }

    vk::PresentModeKHR SwapChainBuilder::choosePresentMode()
    {
        auto presentModes =
            mPhysicalDevice->QuerySwapChainSupport(mSurface->Get()).presentModes;

        auto presentModesByPriotiry =
            mVSync
                ? std::vector<
                      vk::PresentModeKHR>{vk::PresentModeKHR::eFifo,
                                          vk::PresentModeKHR::eMailbox,
                                          vk::PresentModeKHR::eImmediate,
                                          vk::PresentModeKHR::eFifoRelaxed,
                                          vk::PresentModeKHR::eSharedContinuousRefresh,
                                          vk::PresentModeKHR::eSharedDemandRefresh}
                : std::vector<vk::PresentModeKHR>{
                      vk::PresentModeKHR::eMailbox,
                      vk::PresentModeKHR::eImmediate,
                      vk::PresentModeKHR::eFifoRelaxed,
                      vk::PresentModeKHR::eFifo,
                      vk::PresentModeKHR::eSharedContinuousRefresh,
                      vk::PresentModeKHR::eSharedDemandRefresh};

        for (const auto &presentMode : presentModesByPriotiry)
        {
            if (std::find(presentModes.begin(), presentModes.end(), presentMode) !=
                presentModes.end())
            {
                return presentMode;
            }
        }

        return vk::PresentModeKHR::eFifo;
    }

} // namespace FREYA_NAMESPACE
