#pragma once

#include "Freya/Asset/Material.hpp"
#include "Freya/Asset/TexturePool.hpp"
#include "Freya/Containers/SparseSet.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/RenderPass.hpp"

namespace FREYA_NAMESPACE
{
    class MaterialPool
    {
        using MaterialSet = SparseSet<Material>;

      public:
        MaterialPool(const Ref<Device>&                    device,
                     const Ref<CommandPool>&               commandPool,
                     const Ref<RenderPass>&                renderPass,
                     const Ref<TexturePool>&               texturePool,
                     const Ref<skr::Logger<MaterialPool>>& logger) :
            mDevice(device), mCommandPool(commandPool), mRenderPass(renderPass),
            mTexturePool(texturePool), mLogger(logger), mMaterials(4096) {};

        ~MaterialPool() = default;

        std::uint32_t CreateFromTextureFiles(
            std::vector<std::string> texturesPath);

        std::uint32_t Create(std::vector<std::uint32_t> textures);

        void Bind(std::uint32_t materialId);

      private:
        Ref<Device>                    mDevice;
        Ref<CommandPool>               mCommandPool;
        Ref<RenderPass>                mRenderPass;
        Ref<TexturePool>               mTexturePool;
        Ref<skr::Logger<MaterialPool>> mLogger;

        MaterialSet mMaterials;
    };

} // namespace FREYA_NAMESPACE