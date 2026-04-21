#pragma once

#include "Freya/Asset/Texture.hpp"
#include "Freya/Containers/SparseSet.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/RenderPass.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Type definition for Texture SparseSet container.
     */
    using TextureSet = SparseSet<Texture>;

    /**
     * @brief Manages texture creation, sampling, and lifecycle.
     *
     * Creates Vulkan images from file data using STB image library,
     * manages staging buffers, and creates samplers. Textures are
     * stored in a SparseSet for efficient lookup by ID.
     *
     * @param serviceProvider Service provider for logger and builders
     * @param device          Device reference
     * @param commandPool     Command pool for transfer operations
     * @param renderPass      Render pass for sampler descriptor set allocation
     */
    class TexturePool
    {
      public:
        using TextureSet = SparseSet<Texture>;

        TexturePool(const Ref<skr::ServiceProvider>& serviceProvider,
                    const Ref<Device>&               device,
                    const Ref<CommandPool>&          commandPool,
                    const Ref<RenderPass>&           renderPass);

        ~TexturePool();

        /**
         * @brief Creates a texture from an image file.
         * @param path Path to image file (PNG, JPG, etc.)
         * @return Texture ID for later binding
         */
        std::uint32_t CreateTextureFromFile(std::string path);

        /**
         * @brief Returns a texture reference by ID.
         * @param textureId Texture identifier
         * @return Reference to Texture structure
         * @throws Assertion if texture ID doesn't exist
         */
        Texture& GetTexture(std::uint32_t textureId)
        {
            mLogger->Assert(mTextures.contains(textureId),
                            "Failed to get texture with id: {}",
                            textureId);

            return mTextures[textureId];
        }

        /**
         * @brief Finds existing staging buffer with sufficient space or creates
         * new.
         * @param size Required size in bytes
         * @return Staging buffer reference
         */
        Ref<Buffer> queryStagingBuffer(std::uint32_t size);

        /**
         * @brief Creates a new staging buffer.
         * @param size Buffer size in bytes
         * @return Staging buffer reference
         */
        Ref<Buffer> createStagingBuffer(std::uint32_t size);

      private:
        Ref<skr::Logger<TexturePool>> mLogger;
        Ref<skr::ServiceProvider>     mServiceProvider;
        Ref<Device>                   mDevice;
        Ref<CommandPool>              mCommandPool;
        Ref<RenderPass>               mRenderPass;
        std::vector<Ref<Buffer>>      mStagingBuffers;
        vk::DescriptorSet             mTextureDescriptorSet;

        TextureSet mTextures;
    };
} // namespace FREYA_NAMESPACE