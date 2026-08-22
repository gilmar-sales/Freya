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
        Color,             ///< Color attachment image
        Depth,             ///< Depth stencil attachment
        Sampling,          ///< MSAA sampling target
        Texture,           ///< Texture/sampled image
        GBufferAlbedo,     ///< Albedo RGB + 8-bit material ID (R8G8B8A8_UNORM)
        GBufferNormal,     ///< World normal + 2-bit flags (A2B10G10R10)
        GBufferPbr,        ///< Rough, metal, AO|coatR, clearcoat (RGBA8)
        GBufferSceneColor, ///< HDR light accumulation / emissive (RGBA16F)
        GBufferVelocity,   ///< Screen-space motion vectors (RG16F)
        TaaHistory,        ///< TAA history / resolve target (RGBA16F storage)
        TaaDepthHistory,   ///< TAA depth history for disocclusion (R16F)
        HiZDepth,          ///< Hi-Z depth pyramid (R32F storage + sampled)
        Ssao               ///< SSAO buffer (R8 storage + sampled)
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
            const skr::Arc<Surface>&                   surface,
            const skr::Arc<Device>&                    device,
            const skr::Arc<skr::Logger<ImageBuilder>>& logger,
            const skr::Arc<skr::ServiceProvider>&      serviceProvider) :
            mSurface(surface), mDevice(device), mLogger(logger),
            mServiceProvider(serviceProvider), mUsage(ImageUsage::Texture),
            mFormat(vk::Format::eUndefined),
            mSamples(vk::SampleCountFlagBits::e1), mWidth(1024), mHeight(1024),
            mChannels(0), mMipLevels(1), mMipLevelsOverride(false),
            mUploadCustomMipChain(false), mData(nullptr)
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
         * @brief Overrides automatic mip-chain length for textures.
         * @param mipLevels Number of mip levels (minimum 1)
         */
        ImageBuilder& SetMipLevels(const std::uint32_t mipLevels)
        {
            mMipLevels         = mipLevels == 0 ? 1u : mipLevels;
            mMipLevelsOverride = true;
            return *this;
        }

        /**
         * @brief Upload a packed custom mip chain from SetData (no blit).
         *
         * Data layout is tightly packed float/byte texels:
         * mip0 (W×H), mip1 (W/2×H/2), … for SetMipLevels count.
         * Staging size is the sum of all mip byte sizes.
         */
        ImageBuilder& SetUploadCustomMipChain(const bool enable = true)
        {
            mUploadCustomMipChain = enable;
            return *this;
        }

        /**
         * @brief Sets pre-allocated staging buffer.
         * @param stagingBuffer Staging buffer reference
         * @return Reference to this for chaining
         */
        ImageBuilder& SetStagingBuffer(const skr::Arc<Buffer>& stagingBuffer)
        {
            mStagingBuffer = stagingBuffer;
            return *this;
        }

        /**
         * @brief Builds and returns the Image object.
         * @return Shared pointer to created Image
         */
        skr::Arc<Image> Build();

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
        void transitionLayout(
            const skr::Arc<CommandPool>& commandPool, vk::Image image,
            vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
            std::uint32_t baseMipLevel = 0,
            std::uint32_t levelCount   = VK_REMAINING_MIP_LEVELS) const;

      private:
        skr::Arc<skr::Logger<ImageBuilder>> mLogger;  ///< Logger reference
        skr::Arc<Surface>                   mSurface; ///< Surface reference
        skr::Arc<Device>                    mDevice;  ///< Device reference
        skr::Arc<skr::ServiceProvider>
            mServiceProvider; ///< Service provider reference

        skr::Arc<Buffer>
                   mStagingBuffer; ///< Optional pre-allocated staging buffer
        ImageUsage mUsage;         ///< Image usage type

        vk::Format              mFormat;    ///< Image format
        vk::SampleCountFlagBits mSamples;   ///< MSAA sample count
        std::uint32_t           mWidth;     ///< Image width
        std::uint32_t           mHeight;    ///< Image height
        std::uint32_t           mChannels;  ///< Bytes per pixel (upload size)
        std::uint32_t           mMipLevels; ///< Number of mip levels
        bool  mMipLevelsOverride; ///< True when SetMipLevels was called
        bool  mUploadCustomMipChain; ///< Packed mips from SetData; skip blit
        void* mData;                 ///< Raw image data
    };

} // namespace FREYA_NAMESPACE
