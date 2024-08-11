#pragma once

#include "Core/Device.hpp"
#include "Core/Image.hpp"

namespace FREYA_NAMESPACE
{
    enum class ImageUsage
    {
        Color,
        Depth,
        Sampling,
        Texture
    };

    class ImageBuilder
    {
      public:
        ImageBuilder(std::shared_ptr<Device> device) :
            mDevice(device), mUsage(ImageUsage::Texture),
            mFormat(vk::Format::eUndefined), mSamples(vk::SampleCountFlagBits::e1),
            mWidth(1024), mHeight(1024)
        {
        }

        ImageBuilder& SetUsage(ImageUsage usage)
        {
            mUsage = usage;
            return *this;
        }

        ImageBuilder& SetFormat(vk::Format format)
        {
            mFormat = format;
            return *this;
        }

        ImageBuilder& SetSamples(vk::SampleCountFlagBits samples)
        {
            mSamples = samples;
            return *this;
        }

        ImageBuilder& SetWidth(std::uint32_t width)
        {
            mWidth = width;
            return *this;
        }

        ImageBuilder& SetHeight(std::uint32_t height)
        {
            mHeight = height;
            return *this;
        }

        std::shared_ptr<Image> Build();

      protected:
        vk::Format chooseFormat();

      private:
        std::shared_ptr<Device> mDevice;

        ImageUsage mUsage;

        vk::Format              mFormat;
        vk::SampleCountFlagBits mSamples;
        std::uint32_t           mWidth;
        std::uint32_t           mHeight;
    };

} // namespace FREYA_NAMESPACE
