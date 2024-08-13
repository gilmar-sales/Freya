#pragma once

#include "Core/Device.hpp"

namespace FREYA_NAMESPACE
{

    class Image
    {
      public:
        Image(const Ref<Device>&     device,
              const vk::Image        image,
              const vk::ImageView    imageView,
              const vk::DeviceMemory memory,
              const vk::Format       format) :
            mDevice(device), mImage(image), mImageView(imageView), mMemory(memory),
            mFormat(format)
        {
        }

        ~Image();

        vk::Image&        GetImage() { return mImage; }
        vk::ImageView&    GetImageView() { return mImageView; }
        vk::DeviceMemory& GetMemory() { return mMemory; }
        vk::Format&       GetFormat() { return mFormat; }

      private:
        Ref<Device> mDevice;

        vk::Image        mImage;
        vk::ImageView    mImageView;
        vk::DeviceMemory mMemory;
        vk::Format       mFormat;
    };

}; // namespace FREYA_NAMESPACE
