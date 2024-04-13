#pragma once

#include "Core/Device.hpp"

namespace FREYA_NAMESPACE
{

    class Image
    {
      public:
        Image(std::shared_ptr<Device> device,
              vk::Image image,
              vk::ImageView imageView,
              vk::DeviceMemory memory,
              vk::Format format)
            : mDevice(device), mImage(image), mImageView(imageView), mMemory(memory),
              mFormat(format)
        {
        }

        ~Image();

        vk::Image &GetImage() { return mImage; }
        vk::ImageView &GetImageView() { return mImageView; }
        vk::DeviceMemory &GetMemory() { return mMemory; }
        vk::Format &GetFormat() { return mFormat; }

      private:
        std::shared_ptr<Device> mDevice;

        vk::Image mImage;
        vk::ImageView mImageView;
        vk::DeviceMemory mMemory;
        vk::Format mFormat;
    };

}; // namespace FREYA_NAMESPACE
