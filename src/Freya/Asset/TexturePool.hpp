#pragma once

#include "Freya/Asset/Texture.hpp"
#include "Freya/Containers/SparseSet.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/RenderPass.hpp"

namespace FREYA_NAMESPACE
{
    class TexturePool
    {
      public:
        using TextureSet = SparseSet<Texture>;

        TexturePool(const Ref<skr::ServiceProvider>& serviceProvider,
                    const Ref<Device>&               device,
                    const Ref<CommandPool>&          commandPool,
                    const Ref<RenderPass>&           renderPass);

        ~TexturePool();

        std::uint32_t CreateTextureFromFile(std::string path);

        Texture& GetTexture(std::uint32_t textureId)
        {
            mLogger->Assert(mTextures.contains(textureId),
                            "Failed to get texture with id: {}",
                            textureId);

            return mTextures[textureId];
        }

        Ref<Buffer> queryStagingBuffer(std::uint32_t size);
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