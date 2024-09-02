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
                    const Ref<ForwardPass>&  renderPass);

        ~TexturePool();

        std::uint32_t CreateTextureFromFile(std::string path);
        void          Bind(uint32_t uint32);

      private:
        Ref<Device>      mDevice;
        Ref<CommandPool> mCommandPool;
        Ref<ForwardPass>  mRenderPass;

        TextureSet mTextures;
    };
} // namespace FREYA_NAMESPACE