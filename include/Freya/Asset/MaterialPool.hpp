#pragma once

#include "Freya/Asset/Material.hpp"
#include "Freya/Asset/MaterialDescriptorResources.hpp"
#include "Freya/Asset/TexturePool.hpp"
#include "Freya/Containers/SparseSet.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"

namespace FREYA_NAMESPACE
{
    class Renderer;
}

namespace FREYA_NAMESPACE
{
    using MaterialSet = SparseSet<Material>;

    /**
     * @brief Manages materials combining multiple textures.
     */
    class MaterialPool
    {
      public:
        MaterialPool(const skr::Arc<Device>&                      device,
                     const skr::Arc<CommandPool>&                 commandPool,
                     const skr::Arc<MaterialDescriptorResources>& materials,
                     const skr::Arc<TexturePool>&                 texturePool,
                     const skr::Arc<skr::Logger<MaterialPool>>&   logger) :
            mDevice(device), mCommandPool(commandPool),
            mMaterialsRes(materials), mTexturePool(texturePool),
            mLogger(logger), mMaterials(4096) {};

        ~MaterialPool() = default;

        std::uint32_t CreateFromTextureFiles(
            std::vector<std::string> texturesPath);

        std::uint32_t Create(const MaterialCreateInfo& createInfo);

        void Update(std::uint32_t id, const MaterialCreateInfo& createInfo);

        [[nodiscard]] const MaterialCreateInfo& GetCreateInfo(
            std::uint32_t id) const;

      protected:
        friend class FREYA_NAMESPACE::Renderer;
        Material& GetMaterial(uint32_t uint32);

      private:
        void writeTextureDescriptors(Material&                 material,
                                     const MaterialCreateInfo& createInfo);
        void writeFactorsDescriptor(Material& material);
        void uploadFactors(Material& material);
        void writeBindlessMaterial(Material& material);

        skr::Arc<Device>                      mDevice;
        skr::Arc<CommandPool>                 mCommandPool;
        skr::Arc<MaterialDescriptorResources> mMaterialsRes;
        skr::Arc<TexturePool>                 mTexturePool;
        skr::Arc<skr::Logger<MaterialPool>>   mLogger;

        MaterialSet mMaterials;
    };

} // namespace FREYA_NAMESPACE
