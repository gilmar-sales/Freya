#pragma once

#include "Freya/Core/Device.hpp"
#include "Freya/Core/MemoryAllocator.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Wrapper for a VMA-backed Vulkan image.
     *
     * Memory is sub-allocated from MemoryAllocator blocks.
     * Used for depth buffers, MSAA targets, and textures.
     *
     * @param device     Device reference (owns the VMA allocator)
     * @param image      Vulkan image handle (VMA-owned)
     * @param imageView  Vulkan image view handle
     * @param allocation VMA allocation handle
     * @param format     Image format
     */
    class Image
    {
      public:
        Image(const skr::Arc<Device>& device,
              const vk::Image         image,
              const vk::ImageView     imageView,
              const MemoryAllocation  allocation,
              const vk::Format        format,
              const std::uint32_t     mipLevels = 1) :
            mDevice(device), mImage(image), mImageView(imageView),
            mAllocation(allocation), mFormat(format), mMipLevels(mipLevels)
        {
        }

        ~Image();

        /**
         * @brief Returns the underlying image handle.
         */
        vk::Image& GetImage() { return mImage; }

        /**
         * @brief Returns the image view handle.
         */
        vk::ImageView& GetImageView() { return mImageView; }

        /**
         * @brief Returns the VMA allocation handle.
         */
        MemoryAllocation GetAllocation() const { return mAllocation; }

        /**
         * @brief Returns the image format.
         */
        vk::Format& GetFormat() { return mFormat; }

        std::uint32_t GetMipLevels() const { return mMipLevels; }

      private:
        skr::Arc<Device> mDevice;

        vk::Image        mImage;
        vk::ImageView    mImageView;
        MemoryAllocation mAllocation {};
        vk::Format       mFormat;
        std::uint32_t    mMipLevels;
    };

}; // namespace FREYA_NAMESPACE
