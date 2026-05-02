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
         * @return Material ID for bindless texture indexing
         *
         * Updates the global bindless sampler descriptor set at the material's
         * index. The materialId returned is used by the shader to index into
         * the bindless texture arrays via nonuniformEXT().
         */
        std::uint32_t Create(std::vector<std::uint32_t> textures);

        /**
         * @brief Binds the global bindless sampler descriptor set.
         * @param materialId Ignored in bindless mode — kept for API
         *                   compatibility. The material ID is carried in the
         *                   draw metadata buffer and used directly in the
         *                   shader.
         */
        void Bind(std::uint32_t materialId);

        /**
         * @brief Returns the global bindless sampler descriptor set for set 1.
         */
        vk::DescriptorSet GetBindlessSamplerSet() const
        {
            return mBindlessSamplerSet;
        }

      private:
        void ensureBindlessDescriptorSet();

        Ref<Device>                    mDevice;
        Ref<CommandPool>               mCommandPool;
        Ref<RenderPass>                mRenderPass;
        Ref<TexturePool>               mTexturePool;
        Ref<skr::Logger<MaterialPool>> mLogger;

        MaterialSet     mMaterials;
        vk::DescriptorSet mBindlessSamplerSet { VK_NULL_HANDLE };
    };

} // namespace FREYA_NAMESPACE