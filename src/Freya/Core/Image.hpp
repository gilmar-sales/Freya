#pragma once

#include "Freya/Core/Device.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Wrapper for Vulkan image with view and memory management.
     *
     * Manages a 2D Vulkan image, its view, and device memory.
     * Used for depth buffers, MSAA targets, and textures.
     *
     * @param device    Device reference
     * @param image     Vulkan image handle
     * @param imageView Vulkan image view handle
     * @param memory    Device memory handle
     * @param format    Image format
     */
    class Image
    {
      public:
        Image(const Ref<Device>&     device,
              const vk::Image        image,
              const vk::ImageView    imageView,
              const vk::DeviceMemory memory,
              const vk::Format       format) :
            mDevice(device), mImage(image), mImageView(imageView),
            mMemory(memory), mFormat(format)
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
         * @brief Returns the device memory handle.
         */
        vk::DeviceMemory& GetMemory() { return mMemory; }

        /**
         * @brief Returns the image format.
         */
        vk::Format& GetFormat() { return mFormat; }

      private:
        Ref<Device> mDevice;

        vk::Image        mImage;
        vk::ImageView    mImageView;
        vk::DeviceMemory mMemory;
        vk::Format       mFormat;
    };

}; // namespace FREYA_NAMESPACE
