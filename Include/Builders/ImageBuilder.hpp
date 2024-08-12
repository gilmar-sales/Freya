#pragma once

#include "Core/Device.hpp"
#include "Core/Image.hpp"

#include "CommandPoolBuilder.hpp"

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
        ImageBuilder(Ref<Device> device) :
            mDevice(device), mUsage(ImageUsage::Texture),
            mFormat(vk::Format::eUndefined), mSamples(vk::SampleCountFlagBits::e1),
            mWidth(1024), mHeight(1024), mData(nullptr)
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

        ImageBuilder& SetChannels(std::uint32_t channels)
        {
            mChannels = channels;
            return *this;
        }

        ImageBuilder& SetData(void* data)
        {
            mData = data;
            return *this;
        }

        Ref<Image> Build();

      protected:
        vk::Format chooseFormat();
        void       transitionLayout(Ref<fra::CommandPool> commandPool, vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);

      private:
        Ref<Device> mDevice;

        ImageUsage mUsage;

        vk::Format              mFormat;
        vk::SampleCountFlagBits mSamples;
        std::uint32_t           mWidth;
        std::uint32_t           mHeight;
        std::uint32_t           mChannels;
        void*                   mData;
    };

} // namespace FREYA_NAMESPACE
