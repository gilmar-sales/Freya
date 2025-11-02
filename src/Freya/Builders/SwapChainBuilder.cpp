#include "Freya/Builders/SwapChainBuilder.hpp"

#include "Freya/Builders/ImageBuilder.hpp"
#include "Freya/Builders/SurfaceBuilder.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/Core/RenderPass.hpp"


#include <vulkan/vulkan_to_string.hpp>

namespace FREYA_NAMESPACE
{
    Ref<SwapChain> SwapChainBuilder::Build()
    {
        mLogger->LogTrace("Building 'fra::SwapChain':");

        auto surfaceFormat = mSurface->QuerySurfaceFormat();
        auto presentMode   = choosePresentMode();
        auto extent        = mSurface->QueryExtent();

        const auto vkSampleCount =
            static_cast<vk::SampleCountFlagBits>(mFreyaOptions->sampleCount);

        mLogger->LogTrace("\tFrame Count: {}", mFreyaOptions->frameCount);

        mLogger->LogTrace("\tS`urface Format: {}",
                          to_string(surfaceFormat.format));

        mLogger->LogTrace("\tPresent Mode: {}", to_string(presentMode));

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
                .setMinImageCount(mFreyaOptions->frameCount)
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

        mLogger->LogTrace("\tSharing Mode: {}",
                          to_string(createInfo.imageSharingMode));

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

        for (auto index = 0; index < swapChainImages.size(); index++)
        {
            frames[index].image = std::move(swapChainImages[index]);

            imageViewCreateInfo.setImage(frames[index].image);

            frames[index].imageView =
                mDevice->Get().createImageView(imageViewCreateInfo);

            mLogger->Assert(frames[index].imageView,
                            "\tFailed to create image views");
        }

        auto depthImage =
            mServiceProvider->GetService<ImageBuilder>()
                ->SetUsage(ImageUsage::Depth)
                .SetSamples(vkSampleCount)
                .SetWidth(extent.width)
                .SetHeight(extent.height)
                .Build();

        auto sampleImage =
            mServiceProvider->GetService<ImageBuilder>()
                ->SetUsage(ImageUsage::Sampling)
                .SetSamples(vkSampleCount)
                .SetWidth(extent.width)
                .SetHeight(extent.height)
                .Build();

        for (auto index = 0; index < swapChainImages.size(); index++)
        {
            auto attachments =
                vkSampleCount != vk::SampleCountFlagBits::e1
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

        auto imageAvailableSemaphores =
            std::vector<vk::Semaphore>(mFreyaOptions->frameCount);

        auto renderFinishedSemaphores =
            std::vector<vk::Semaphore>(mFreyaOptions->frameCount);

        auto inFlightFences = std::vector<vk::Fence>(mFreyaOptions->frameCount);

        constexpr auto semaphoreInfo = vk::SemaphoreCreateInfo();

        constexpr auto fenceInfo =
            vk::FenceCreateInfo().setFlags(vk::FenceCreateFlagBits::eSignaled);

        for (size_t i = 0; i < mFreyaOptions->frameCount; i++)
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

        return skr::MakeRef<SwapChain>(
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
            mFreyaOptions->vSync
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
