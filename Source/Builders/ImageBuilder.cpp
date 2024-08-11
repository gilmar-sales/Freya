#include "Builders/ImageBuilder.hpp"

namespace FREYA_NAMESPACE
{
    std::shared_ptr<Image> ImageBuilder::Build()
    {
        if (mFormat == vk::Format::eUndefined)
        {
            chooseFormat();
        }

        auto imageInfo =
            vk::ImageCreateInfo()
                .setExtent(vk::Extent3D().setWidth(mWidth).setHeight(mHeight).setDepth(1))
                .setFormat(mFormat)
                .setTiling(vk::ImageTiling::eOptimal)
                .setMipLevels(1)
                .setArrayLayers(1)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setImageType(vk::ImageType::e2D)
                .setSharingMode(vk::SharingMode::eExclusive)
                .setSamples(mSamples);

        switch (mUsage)
        {
            case ImageUsage::Color:
                imageInfo.setUsage(vk::ImageUsageFlagBits::eColorAttachment);
                break;
            case ImageUsage::Depth:
                imageInfo.setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment);
                break;
            case ImageUsage::Texture:
                imageInfo.setUsage(vk::ImageUsageFlagBits::eSampled);
                break;
            case ImageUsage::Sampling:
                imageInfo.setUsage(vk::ImageUsageFlagBits::eColorAttachment |
                                   vk::ImageUsageFlagBits::eTransientAttachment);
                break;
            default:
                break;
        }

        auto image = mDevice->Get().createImage(imageInfo);

        auto imageRequirements = mDevice->Get().getImageMemoryRequirements(image);

        auto memoryTypeIndex = mDevice->GetPhysicalDevice()->QueryCompatibleMemoryType(
            imageRequirements.memoryTypeBits,
            vk::MemoryPropertyFlagBits::eDeviceLocal);

        auto imageMemoryInfo =
            vk::MemoryAllocateInfo()
                .setAllocationSize(imageRequirements.size)
                .setMemoryTypeIndex(memoryTypeIndex);

        auto imageMemory = mDevice->Get().allocateMemory(imageMemoryInfo);

        mDevice->Get().bindImageMemory(image, imageMemory, 0);

        auto aspect = vk::ImageAspectFlagBits::eNone;

        switch (mUsage)
        {
            case ImageUsage::Color:
            case ImageUsage::Sampling:
            case ImageUsage::Texture:
                aspect = vk::ImageAspectFlagBits::eColor;
                break;
            case ImageUsage::Depth:
                aspect = vk::ImageAspectFlagBits::eDepth;
                break;
            default:
                break;
        }

        auto imageViewInfo =
            vk::ImageViewCreateInfo()
                .setImage(image)
                .setViewType(vk::ImageViewType::e2D)
                .setFormat(mFormat)
                .setComponents(vk::ComponentMapping()
                                   .setR(vk::ComponentSwizzle::eIdentity)
                                   .setG(vk::ComponentSwizzle::eIdentity)
                                   .setB(vk::ComponentSwizzle::eIdentity)
                                   .setA(vk::ComponentSwizzle::eIdentity))
                .setSubresourceRange(vk::ImageSubresourceRange()
                                         .setAspectMask(aspect)
                                         .setBaseMipLevel(0)
                                         .setLevelCount(1)
                                         .setBaseArrayLayer(0)
                                         .setLayerCount(1));

        auto imageView = mDevice->Get().createImageView(imageViewInfo);

        return std::make_shared<Image>(mDevice, image, imageView, imageMemory, mFormat);
    }

    vk::Format ImageBuilder::chooseFormat()
    {
        switch (mUsage)
        {
            case ImageUsage::Color:
                mFormat = mDevice->GetSurface()->QuerySurfaceFormat().format;
                break;
            case ImageUsage::Depth:
                mFormat = mDevice->GetPhysicalDevice()->GetDepthFormat();
                break;
            case ImageUsage::Texture:
                mFormat = vk::Format::eR8G8B8A8Srgb;
                break;
            case ImageUsage::Sampling:
                mFormat = mDevice->GetSurface()->QuerySurfaceFormat().format;
                break;
            default:
                break;
        }

        return vk::Format();
    }
}; // namespace FREYA_NAMESPACE
