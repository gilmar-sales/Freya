#pragma once

#include "Freya/Asset/Material.hpp"
#include "Freya/Asset/TexturePool.hpp"
#include "Freya/Containers/SparseSet.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/RenderPass.hpp"

namespace FREYA_NAMESPACE
{
    class Renderer;
}

namespace FREYA_NAMESPACE
{
    /**
     * @brief Type definition for Material SparseSet container.
     */
    using MaterialSet = SparseSet<Material>;

    /**
     * @brief Manages materials combining multiple textures.
     *
     * Creates materials by combining texture IDs, allocates descriptor
     * sets with combined image samplers + factors UBO, and binds materials
     * to command buffers during rendering.
     *
     * @param device      Device reference
     * @param commandPool Command pool reference
     * @param renderPass  Render pass for sampler descriptor pool access
     * @param texturePool Texture pool for texture lookups
     * @param logger      Logger reference
     */
    class MaterialPool
    {
      public:
        MaterialPool(const skr::Arc<Device>&                    device,
                     const skr::Arc<CommandPool>&               commandPool,
                     const skr::Arc<RenderPass>&                renderPass,
                     const skr::Arc<TexturePool>&               texturePool,
                     const skr::Arc<skr::Logger<MaterialPool>>& logger) :
            mDevice(device), mCommandPool(commandPool), mRenderPass(renderPass),
            mTexturePool(texturePool), mLogger(logger), mMaterials(4096) {};

        ~MaterialPool() = default;

        /**
         * @brief Creates a material from multiple texture file paths.
         * @param texturesPath Vector of texture file paths
         * @return Material ID
         * @note Currently returns 0 (not implemented)
         */
        std::uint32_t CreateFromTextureFiles(
            std::vector<std::string> texturesPath);

        /**
         * @brief Creates a material from existing texture IDs and factors.
         * @param createInfo Material creation parameters
         * @return Material ID for later binding
         */
        std::uint32_t Create(const MaterialCreateInfo& createInfo);

        /**
         * @brief Updates textures and/or factors for an existing material.
         * @param id         Material ID
         * @param createInfo New parameters (textures rewritten if IDs change)
         */
        void Update(std::uint32_t id, const MaterialCreateInfo& createInfo);

        /**
         * @brief Returns the last create/update parameters for a material.
         * @param id Material ID
         */
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

        skr::Arc<Device>                    mDevice;
        skr::Arc<CommandPool>               mCommandPool;
        skr::Arc<RenderPass>                mRenderPass;
        skr::Arc<TexturePool>               mTexturePool;
        skr::Arc<skr::Logger<MaterialPool>> mLogger;

        MaterialSet mMaterials;
    };

} // namespace FREYA_NAMESPACE
