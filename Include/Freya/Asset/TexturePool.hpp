#pragma once

#include "Freya/Asset/Texture.hpp"
#include "Freya/Containers/SparseSet.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/ForwardPass.hpp"

namespace FREYA_NAMESPACE
{
    class TexturePool
    {
      public:
        using TextureSet = SparseSet<Texture>;

        TexturePool(const Ref<skr::ServiceProvider>& serviceProvider,
                    const Ref<Device>&               device,
                    const Ref<CommandPool>&          commandPool,
                    const Ref<ForwardPass>&          renderPass);

        ~TexturePool();

        std::uint32_t CreateTextureFromFile(std::string path);
        void          Bind(uint32_t uint32);

        Ref<Buffer> queryStagingBuffer(std::uint32_t size);
        Ref<Buffer> createStagingBuffer(std::uint32_t size);

      private:
        Ref<skr::Logger>          mLogger;
        Ref<skr::ServiceProvider> mServiceProvider;
        Ref<Device>               mDevice;
        Ref<CommandPool>          mCommandPool;
        Ref<ForwardPass>          mRenderPass;
        std::vector<Ref<Buffer>>  mStagingBuffers;

        TextureSet mTextures;
    };
} // namespace FREYA_NAMESPACE