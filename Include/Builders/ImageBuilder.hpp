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
        explicit ImageBuilder(const Ref<Device>& device) :
            mDevice(device), mUsage(ImageUsage::Texture),
            mFormat(vk::Format::eUndefined), mSamples(vk::SampleCountFlagBits::e1),
            mWidth(1024), mHeight(1024), mChannels(0), mData(nullptr)
        {
        }

        ImageBuilder& SetUsage(const ImageUsage usage)
        {
            mUsage = usage;
            return *this;
        }

        ImageBuilder& SetFormat(const vk::Format format)
        {
            mFormat = format;
            return *this;
        }

        ImageBuilder& SetSamples(const vk::SampleCountFlagBits samples)
        {
            mSamples = samples;
            return *this;
        }

        ImageBuilder& SetWidth(const std::uint32_t width)
        {
            mWidth = width;
            return *this;
        }

        ImageBuilder& SetHeight(const std::uint32_t height)
        {
            mHeight = height;
            return *this;
        }

        ImageBuilder& SetChannels(const std::uint32_t channels)
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
        void       transitionLayout(const Ref<CommandPool>& commandPool, vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) const;

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
