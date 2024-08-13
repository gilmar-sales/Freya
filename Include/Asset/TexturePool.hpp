#pragma once

#include "Containers/SparseSet.hpp"
#include "Core/Buffer.hpp"
#include "Core/CommandPool.hpp"
#include "Core/Device.hpp"
#include "Texture.hpp"

namespace FREYA_NAMESPACE
{
    class TexturePool
    {
      public:
        using TextureSet = SparseSet<Texture>;

        TexturePool(const Ref<Device>& device,
                    const Ref<CommandPool>& commandPool);

        std::uint32_t CreateTextureFromFile(std::string path) const;

      private:
        Ref<Device>      mDevice;
        Ref<CommandPool> mCommandPool;

        std::vector<Ref<Buffer>> mImageBuffers;
        TextureSet               mTextures;
    };
} // namespace FREYA_NAMESPACE