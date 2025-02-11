#pragma once

#include "Containers/SparseSet.hpp"
#include "Core/CommandPool.hpp"
#include "Core/Device.hpp"
#include "Core/ForwardPass.hpp"
#include "Texture.hpp"

namespace FREYA_NAMESPACE
{
    class TexturePool
    {
      public:
        using TextureSet = SparseSet<Texture>;

        TexturePool(const Ref<Device>&      device,
                    const Ref<CommandPool>& commandPool,
                    const Ref<ForwardPass>& renderPass);

        ~TexturePool();

        std::uint32_t CreateTextureFromFile(std::string path);
        void          Bind(uint32_t uint32);

        Ref<Buffer> queryStagingBuffer(std::uint32_t size);
        Ref<Buffer> createStagingBuffer(std::uint32_t size);

      private:
        Ref<Device>              mDevice;
        Ref<CommandPool>         mCommandPool;
        Ref<ForwardPass>         mRenderPass;
        std::vector<Ref<Buffer>> mStagingBuffers;

        TextureSet mTextures;
    };
} // namespace FREYA_NAMESPACE