#pragma once

#include "Freya/Builders/CommandPoolBuilder.hpp"
#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/Image.hpp"
#include "Freya/Core/Surface.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Image usage type enumeration.
     */
    enum class ImageUsage
    {
        Color,           ///< Color attachment image
        Depth,           ///< Depth stencil attachment
        Sampling,        ///< MSAA sampling target
        Texture,         ///< Texture/sampled image
        GBufferPosition, ///< Deferred G-buffer position
        GBufferNormal,   ///< Deferred G-buffer normal
        GBufferAlbedo    ///< Deferred G-buffer albedo
    };

    /**
     * @brief Fluent builder for creating Image objects.
     *
     * Configures dimensions, format, usage, and optional data.
     * Handles staging buffer creation and image layout transitions.
     *
     * @param surface        Surface reference for format queries
     * @param device          Device reference
     * @param logger          Logger reference
     * @param serviceProvider Service provider
     */
    class ImageBuilder
    {
      public:
        /**
         * @brief Constructs builder with required dependencies.
         * @param surface        Surface reference for format queries
         * @param device          Device reference
         * @param logger          Logger reference
         * @param serviceProvider Service provider reference
         */
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

        /**
         * @brief Sets image usage type.
         * @param usage Image usage type
         * @return Reference to this for chaining
         */
        ImageBuilder& SetUsage(const ImageUsage usage)
        {
            mUsage = usage;
            return *this;
        }

        /**
         * @brief Sets image format.
         * @param format Vulkan format
         * @return Reference to this for chaining
         */
        ImageBuilder& SetFormat(const vk::Format format)
        {
            mFormat = format;
            return *this;
        }

        /**
         * @brief Sets MSAA sample count.
         * @param samples Sample count flag
         * @return Reference to this for chaining
         */
        ImageBuilder& SetSamples(const vk::SampleCountFlagBits samples)
        {
            mSamples = samples;
            return *this;
        }

        /**
         * @brief Sets image width in pixels.
         * @param width Width in pixels
         * @return Reference to this for chaining
         */
        ImageBuilder& SetWidth(const std::uint32_t width)
        {
            mWidth = width;
            return *this;
        }

        /**
         * @brief Sets image height in pixels.
         * @param height Height in pixels
         * @return Reference to this for chaining
         */
        ImageBuilder& SetHeight(const std::uint32_t height)
        {
            mHeight = height;
            return *this;
        }

        /**
         * @brief Sets channel count.
         * @param channels Channel count
         * @return Reference to this for chaining
         */
        ImageBuilder& SetChannels(const std::uint32_t channels)
        {
            mChannels = channels;
            return *this;
        }

        /**
         * @brief Sets raw image data pointer.
         * @param data Pointer to image data
         * @return Reference to this for chaining
         */
        ImageBuilder& SetData(void* data)
        {
            mData = data;
            return *this;
        }

        /**
         * @brief Sets pre-allocated staging buffer.
         * @param stagingBuffer Staging buffer reference
         * @return Reference to this for chaining
         */
        ImageBuilder& SetStagingBuffer(const Ref<Buffer>& stagingBuffer)
        {
            mStagingBuffer = stagingBuffer;
            return *this;
        }

        /**
         * @brief Builds and returns the Image object.
         * @return Shared pointer to created Image
         */
        Ref<Image> Build();

      protected:
        /**
         * @brief Chooses format based on usage if not explicitly set.
         * @return Selected format
         */
        vk::Format chooseFormat();

        /**
         * @brief Transitions image layout via pipeline barrier.
         * @param commandPool Command pool for transition
         * @param image       Image to transition
         * @param oldLayout   Current layout
         * @param newLayout   Target layout
         */
        void transitionLayout(const Ref<CommandPool>& commandPool,
                              vk::Image image, vk::ImageLayout oldLayout,
                              vk::ImageLayout newLayout) const;

      private:
        Ref<skr::Logger<ImageBuilder>> mLogger;  ///< Logger reference
        Ref<Surface>                   mSurface; ///< Surface reference
        Ref<Device>                    mDevice;  ///< Device reference
        Ref<skr::ServiceProvider>
            mServiceProvider; ///< Service provider reference

        Ref<Buffer> mStagingBuffer; ///< Optional pre-allocated staging buffer
        ImageUsage  mUsage;         ///< Image usage type

        vk::Format              mFormat;   ///< Image format
        vk::SampleCountFlagBits mSamples;  ///< MSAA sample count
        std::uint32_t           mWidth;    ///< Image width
        std::uint32_t           mHeight;   ///< Image height
        std::uint32_t           mChannels; ///< Channel count
        void*                   mData;     ///< Raw image data
    };

} // namespace FREYA_NAMESPACE
