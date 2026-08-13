#include "Freya/Builders/SwapChainBuilder.hpp"

#include "Freya/Builders/SurfaceBuilder.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/PhysicalDevice.hpp"

#include <vulkan/vulkan_to_string.hpp>

namespace FREYA_NAMESPACE
{
    skr::Arc<SwapChain> SwapChainBuilder::Build()
    {
        mLogger->LogTrace("Building 'fra::SwapChain':");

        auto surfaceFormat = mSurface->QuerySurfaceFormat();
        auto presentMode   = choosePresentMode();
        auto extent        = mSurface->QueryExtent();

        mLogger->LogTrace("\tFrame Count: {}", mFreyaOptions->frameCount);

        mLogger->LogTrace("\tSurface Format: {}",
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

        auto imageAvailableSemaphores =
            std::vector<vk::Semaphore>(mFreyaOptions->frameCount);

        auto renderFinishedSemaphores =
            std::vector<vk::Semaphore>(swapChainImages.size());

        auto inFlightFences = std::vector<vk::Fence>(mFreyaOptions->frameCount);

        constexpr auto semaphoreInfo = vk::SemaphoreCreateInfo();

        constexpr auto fenceInfo =
            vk::FenceCreateInfo().setFlags(vk::FenceCreateFlagBits::eSignaled);

        for (size_t i = 0; i < mFreyaOptions->frameCount; i++)
        {
            imageAvailableSemaphores[i] =
                mDevice->Get().createSemaphore(semaphoreInfo);

            inFlightFences[i] = mDevice->Get().createFence(fenceInfo);
        }

        for (size_t i = 0; i < swapChainImages.size(); i++)
        {
            renderFinishedSemaphores[i] =
                mDevice->Get().createSemaphore(semaphoreInfo);

            mLogger->Assert(
                imageAvailableSemaphores
                        [i < imageAvailableSemaphores.size() ? i : 0] &&
                    renderFinishedSemaphores[i] &&
                    inFlightFences[i < inFlightFences.size() ? i : 0],
                "\tFailed to create synchronization objects for a frame");
        }

        return skr::MakeArc<SwapChain>(
            mDevice,
            mInstance,
            mSurface,
            swapChain,
            frames,
            imageAvailableSemaphores,
            renderFinishedSemaphores,
            inFlightFences);
    }

    vk::PresentModeKHR SwapChainBuilder::choosePresentMode()
    {
        auto presentModes =
            mPhysicalDevice->QuerySwapChainSupport(mSurface->Get())
                .presentModes;

        if (mPreferredPresentMode)
        {
            if (std::ranges::find(presentModes.begin(),
                                  presentModes.end(),
                                  *mPreferredPresentMode) != presentModes.end())
            {
                return *mPreferredPresentMode;
            }
        }

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
