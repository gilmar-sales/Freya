#pragma once

#include "Freya/Builders/CommandPoolBuilder.hpp"
#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/Core/Surface.hpp"

namespace FREYA_NAMESPACE
{
    enum class ImageUsage
    {
        Color,
        Depth,
        Sampling,
        Texture,
        GBufferAlbedo,
        GBufferPosition,
        GBufferNormal,
    };

    class ImageBuilder
    {
      public:
        explicit ImageBuilder(
            const Ref<Surface>&                   surface,
            const Ref<Device>&                    device,
            const Ref<skr::Logger<ImageBuilder>>& logger,
            const Ref<skr::ServiceProvider>&      serviceProvider) :
            mSurface(surface), mDevice(device), mLogger(logger),
            mServiceProvider(serviceProvider), mUsage(ImageUsage::Texture),
            mFormat(vk::Format::eUndefined),
            mSamples(vk::SampleCountFlagBits::e1), mWidth(1024), mHeight(1024),
            mChannels(0), mData(nullptr)
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

        ImageBuilder& SetStagingBuffer(const Ref<Buffer>& stagingBuffer)
        {
            mStagingBuffer = stagingBuffer;
            return *this;
        }

        Ref<Image> Build();

      protected:
        vk::Format chooseFormat();
        void       transitionLayout(const Ref<CommandPool>& commandPool,
                                    vk::Image image, vk::ImageLayout oldLayout,
                                    vk::ImageLayout newLayout) const;

      private:
        Ref<skr::Logger<ImageBuilder>> mLogger;
        Ref<Surface>                   mSurface;
        Ref<Device>                    mDevice;
        Ref<skr::ServiceProvider>      mServiceProvider;

        Ref<Buffer> mStagingBuffer;
        ImageUsage  mUsage;

        vk::Format              mFormat;
        vk::SampleCountFlagBits mSamples;
        std::uint32_t           mWidth;
        std::uint32_t           mHeight;
        std::uint32_t           mChannels;
        void*                   mData;
    };

} // namespace FREYA_NAMESPACE
