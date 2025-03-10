#include "Freya/Builders/SwapChainBuilder.hpp"

#include "Freya/Builders/ImageBuilder.hpp"
#include "Freya/Builders/SurfaceBuilder.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/ForwardPass.hpp"
#include "Freya/Core/PhysicalDevice.hpp"

#include <vulkan/vulkan_to_string.hpp>

namespace FREYA_NAMESPACE
{
    Ref<SwapChain> SwapChainBuilder::Build()
    {
        mLogger->LogTrace("Building 'fra::SwapChain':");

        auto surfaceFormat = mSurface->QuerySurfaceFormat();
        auto presentMode   = choosePresentMode();
        auto extent        = mSurface->QueryExtent();

        mLogger->LogTrace("\tFrame Count: {}", mFrameCount);

        auto surfaceFormatString = to_string(surfaceFormat.format);
        mLogger->LogTrace("\tSurface Format: {}", surfaceFormatString);

        auto presentModeString = to_string(presentMode);
        mLogger->LogTrace("\tPresent Mode: {}", presentModeString);

        auto supportDetails =
            mPhysicalDevice->QuerySwapChainSupport(mSurface->Get());

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

        mLogger->Assert(
            mDevice->GetQueueFamilyIndices().isComplete(),
            "Could not set image sharing mode with incomplete queue families");

        if (mDevice->GetQueueFamilyIndices().isUnique())
        {
            std::uint32_t queueFamilyIndices[] = {
                mDevice->GetQueueFamilyIndices().graphicsFamily.value(),
                mDevice->GetQueueFamilyIndices().presentFamily.value()
            };

            createInfo.setImageSharingMode(vk::SharingMode::eConcurrent)
                .setQueueFamilyIndices(queueFamilyIndices);
        }

        auto sharingMode = to_string(createInfo.imageSharingMode);
        mLogger->LogTrace("\tSharing Mode: {}", sharingMode);

        auto swapChain = mDevice->Get().createSwapchainKHR(createInfo);

        mLogger->Assert(swapChain, "\tFailed to create swap chain");

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
                .setSubresourceRange(
                    vk::ImageSubresourceRange()
                        .setAspectMask(vk::ImageAspectFlagBits::eColor)
                        .setBaseMipLevel(0)
                        .setLevelCount(1)
                        .setBaseArrayLayer(0)
                        .setLayerCount(1));

        auto frames = std::vector<SwapChainFrame>(swapChainImages.size());

        auto depthImage =
            mServiceProvider->GetService<ImageBuilder>()
                ->SetDevice(mDevice)
                .SetUsage(ImageUsage::Depth)
                .SetSamples(mSamples)
                .SetWidth(extent.width)
                .SetHeight(extent.height)
                .Build();

        auto sampleImage =
            mServiceProvider->GetService<ImageBuilder>()
                ->SetDevice(mDevice)
                .SetUsage(ImageUsage::Sampling)
                .SetSamples(mSamples)
                .SetWidth(extent.width)
                .SetHeight(extent.height)
                .Build();

        for (auto index = 0; index < swapChainImages.size(); index++)
        {
            frames[index].image = swapChainImages[index];

            imageViewCreateInfo.setImage(swapChainImages[index]);
            frames[index].imageView =
                mDevice->Get().createImageView(imageViewCreateInfo);

            mLogger->Assert(frames[index].imageView,
                            "\tFailed to create image views");

            auto attachments =
                mSamples != vk::SampleCountFlagBits::e1
                    ? std::vector<vk::ImageView> { sampleImage->GetImageView(),
                                                   depthImage->GetImageView(),
                                                   frames[index].imageView }
                    : std::vector<vk::ImageView> { frames[index].imageView,
                                                   depthImage->GetImageView() };

            auto framebufferInfo =
                vk::FramebufferCreateInfo()
                    .setRenderPass(mRenderPass->Get())
                    .setAttachments(attachments)
                    .setWidth(extent.width)
                    .setHeight(extent.height)
                    .setLayers(1);

            frames[index].frameBuffer =
                mDevice->Get().createFramebuffer(framebufferInfo);

            mLogger->Assert(frames[index].frameBuffer,
                            "\tFailed to create framebuffer");
        }

        auto imageAvailableSemaphores = std::vector<vk::Semaphore>(mFrameCount);

        auto renderFinishedSemaphores = std::vector<vk::Semaphore>(mFrameCount);

        auto inFlightFences = std::vector<vk::Fence>(mFrameCount);

        constexpr auto semaphoreInfo = vk::SemaphoreCreateInfo();

        constexpr auto fenceInfo =
            vk::FenceCreateInfo().setFlags(vk::FenceCreateFlagBits::eSignaled);

        for (size_t i = 0; i < mFrameCount; i++)
        {
            imageAvailableSemaphores[i] =
                mDevice->Get().createSemaphore(semaphoreInfo);

            renderFinishedSemaphores[i] =
                mDevice->Get().createSemaphore(semaphoreInfo);

            inFlightFences[i] = mDevice->Get().createFence(fenceInfo);

            mLogger->Assert(
                imageAvailableSemaphores[i] && renderFinishedSemaphores[i] &&
                    inFlightFences[i],
                "\tFailed to create synchronization objects for a frame");
        }

        return MakeRef<SwapChain>(
            mDevice,
            mInstance,
            mSurface,
            swapChain,
            frames,
            depthImage,
            sampleImage,
            imageAvailableSemaphores,
            renderFinishedSemaphores,
            inFlightFences);
    }

    vk::PresentModeKHR SwapChainBuilder::choosePresentMode()
    {
        auto presentModes =
            mPhysicalDevice->QuerySwapChainSupport(mSurface->Get())
                .presentModes;

        const auto presentModesByPriotiry =
            mVSync
                ? std::vector { vk::PresentModeKHR::eFifo,
                                vk::PresentModeKHR::eMailbox,
                                vk::PresentModeKHR::eImmediate,
                                vk::PresentModeKHR::eFifoRelaxed,
                                vk::PresentModeKHR::eSharedContinuousRefresh,
                                vk::PresentModeKHR::eSharedDemandRefresh }
                : std::vector { vk::PresentModeKHR::eMailbox,
                                vk::PresentModeKHR::eImmediate,
                                vk::PresentModeKHR::eFifoRelaxed,
                                vk::PresentModeKHR::eFifo,
                                vk::PresentModeKHR::eSharedContinuousRefresh,
                                vk::PresentModeKHR::eSharedDemandRefresh };

        for (const auto& presentMode : presentModesByPriotiry)
        {
            if (std::ranges::find(presentModes.begin(),
                                  presentModes.end(),
                                  presentMode) != presentModes.end())
            {
                return presentMode;
            }
        }

        return vk::PresentModeKHR::eFifo;
    }
} // namespace FREYA_NAMESPACE
