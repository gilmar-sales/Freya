#pragma once

#include "Freya/Asset/Material.hpp"
#include "Freya/Asset/MaterialDescriptorResources.hpp"
#include "Freya/Containers/SparseSet.hpp"

#include <string>
#include <vector>

namespace FREYA_NAMESPACE
{
    using MaterialSet = SparseSet<Material>;

    /**
     * @brief Manages materials combining multiple textures.
     */
    class MaterialPool
    {
      public:
        MaterialPool(const skr::Arc<MaterialDescriptorResources>& materials,
                     const skr::Arc<skr::Logger<MaterialPool>>&   logger) :
            mMaterialsRes(materials), mLogger(logger), mMaterials(4096) {};

        ~MaterialPool() = default;

        std::uint32_t CreateFromTextureFiles(
            [[maybe_unused]] std::vector<std::string> texturesPath);

        std::uint32_t Create(const MaterialCreateInfo& createInfo);

        void Update(std::uint32_t id, const MaterialCreateInfo& createInfo);

        [[nodiscard]] const MaterialCreateInfo& GetCreateInfo(
            std::uint32_t id) const;

      private:
        void writeBindlessMaterial(Material& material);

        skr::Arc<MaterialDescriptorResources> mMaterialsRes;
        skr::Arc<skr::Logger<MaterialPool>>   mLogger;

        MaterialSet mMaterials;
    };

} // namespace FREYA_NAMESPACE
