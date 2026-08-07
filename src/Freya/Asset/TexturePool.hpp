#pragma once

#include "Freya/Asset/MaterialDescriptorResources.hpp"
#include "Freya/Asset/Texture.hpp"
#include "Freya/Containers/SparseSet.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"

namespace FREYA_NAMESPACE
{
    using TextureSet = SparseSet<Texture>;

    /**
     * @brief Manages texture creation, sampling, and lifecycle.
     */
    class TexturePool
    {
      public:
        using TextureSet = SparseSet<Texture>;

        TexturePool(const skr::Arc<skr::ServiceProvider>& serviceProvider,
                    const skr::Arc<Device>&               device,
                    const skr::Arc<CommandPool>&          commandPool,
                    const skr::Arc<MaterialDescriptorResources>& materials);

        ~TexturePool();

        std::uint32_t CreateTextureFromFile(std::string path);

        Texture& GetTexture(std::uint32_t textureId)
        {
            mLogger->Assert(mTextures.contains(textureId),
                            "Failed to get texture with id: {}",
                            textureId);

            return mTextures[textureId];
        }

        skr::Arc<Buffer> queryStagingBuffer(std::uint32_t size);

        skr::Arc<Buffer> createStagingBuffer(std::uint32_t size);

      private:
        skr::Arc<skr::Logger<TexturePool>>    mLogger;
        skr::Arc<skr::ServiceProvider>        mServiceProvider;
        skr::Arc<Device>                      mDevice;
        skr::Arc<CommandPool>                 mCommandPool;
        skr::Arc<MaterialDescriptorResources> mMaterialsRes;
        std::vector<skr::Arc<Buffer>>         mStagingBuffers;
        vk::DescriptorSet                     mTextureDescriptorSet;

        TextureSet mTextures;
    };
} // namespace FREYA_NAMESPACE
