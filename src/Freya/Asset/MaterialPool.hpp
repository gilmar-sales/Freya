#pragma once

#include "Freya/Asset/Material.hpp"
#include "Freya/Asset/TexturePool.hpp"
#include "Freya/Containers/SparseSet.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/RenderPass.hpp"

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
     * sets with combined image samplers, and binds materials to command
     * buffers during rendering.
     *
     * @param device      Device reference
     * @param commandPool Command pool reference
     * @param renderPass  Render pass for sampler descriptor pool access
     * @param texturePool Texture pool for texture lookups
     * @param logger      Logger reference
     */
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

        /**
         * @brief Creates a material from multiple texture file paths.
         * @param texturesPath Vector of texture file paths
         * @return Material ID
         * @note Currently returns 0 (not implemented)
         */
        std::uint32_t CreateFromTextureFiles(
            std::vector<std::string> texturesPath);

        /**
         * @brief Creates a material from existing texture IDs.
         * @param textures Vector of texture IDs
         * @return Material ID for later binding
         */
        std::uint32_t Create(std::vector<std::uint32_t> textures);

        /**
         * @brief Binds material's descriptor sets to the command buffer.
         * @param materialId Material identifier
         */
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