#pragma once

#include "Freya/Asset/Texture.hpp"

#include <Skirnir/Skirnir.hpp>

#include "Freya/Asset/LODPool.hpp"
#include "Freya/Asset/LODTypes.hpp"
#include "Freya/Asset/MeshPool.hpp"
#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/Renderer.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{

    /**
     * @brief Main LOD service that manages GPU-driven LOD selection and culling
     *
     * LODService orchestrates the LOD system:
     * 1. Maintains instance data (which mesh group each instance uses)
     * 2. Provides GPU buffers for compute shader to read/write
     * 3. Handles transform buffer binding
     * 4. Records indirect draw commands
     *
     * Usage:
     * @code
     * auto lodService = serviceProvider->GetService<LODService>();
     *
     * // Create LOD group
     * uint32_t groupId = lodService->CreateLODGroup(meshIds, distances);
     *
     * // Add instances
     * uint32_t instanceId = lodService->AddInstance(groupId, transformIndex);
     *
     * // Per frame:
     * lodService->UpdateGPUData(cmd, cameraPos);
     * lodService->RecordDrawCommands(cmd);
     * @endcode
     */
    class LODService
    {
      public:
        LODService(const Ref<skr::ServiceProvider>& serviceProvider);

        ~LODService();

        // === Instance Management ===

        /**
         * @brief Adds a new LOD-managed instance
         * @param groupId LOD group to use
         * @param transformIndex Index into transform buffer
         * @return Instance ID for later manipulation
         */
        std::uint32_t AddInstance(std::uint32_t groupId,
                                  std::uint32_t transformIndex);

        /**
         * @brief Updates an instance's transform index
         */
        void UpdateInstance(std::uint32_t instanceId,
                            std::uint32_t transformIndex);

        /**
         * @brief Removes an instance
         */
        void RemoveInstance(std::uint32_t instanceId);

        /**
         * @brief Updates a transform in the transform buffer
         */
        void UpdateTransform(std::uint32_t    transformIndex,
                             const glm::mat4& transform);

        // === GPU Data Update ===

        /**
         * @brief Updates GPU buffers with latest instance data
         * @param cmd Command buffer to record upload
         * @param cameraPosition Current camera world position
         */
        void UpdateGPUData(vk::CommandBuffer cmd,
                           const glm::vec3&  cameraPosition);

        // === Rendering ===

        /**
         * @brief Records indirect draw commands to command buffer
         * @param cmd Command buffer to record to
         */
        void RecordDrawCommands(vk::CommandBuffer cmd);

        /**
         * @brief Binds the LOD descriptor set
         */
        void BindDescriptorSet(vk::CommandBuffer  cmd,
                               vk::PipelineLayout layout);

        // === Configuration ===

        void SetGlobalDrawDistance(float distance);
        void SetLODDistance(std::uint32_t groupId,
                            std::uint32_t level,
                            float         distance);

        // === Accessors ===

        [[nodiscard]] vk::Buffer GetInstanceBuffer() const
        {
            return mInstanceBuffer->Get();
        }
        [[nodiscard]] vk::Buffer GetDrawCommandBuffer() const
        {
            return mDrawCommandBuffer->Get();
        }
        [[nodiscard]] vk::Buffer GetTransformBuffer() const
        {
            return mTransformBuffer->Get();
        }
        [[nodiscard]] std::uint32_t GetInstanceCount() const
        {
            return mInstanceCount;
        }
        [[nodiscard]] std::uint32_t GetMaxInstances() const
        {
            return mMaxInstances;
        }
        [[nodiscard]] VkDescriptorSet GetDescriptorSet() const
        {
            return mLODDescriptorSet;
        }

      private:
        void createGPUBuffers();
        void createDescriptorSetLayout();
        void allocateDescriptorSet();
        void createComputePipeline();
        void createDitherTexture();
        void updateDrawCount(vk::CommandBuffer cmd);

        Ref<Device>                  mDevice;
        Ref<PhysicalDevice>          mPhysicalDevice;
        Ref<CommandPool>             mCommandPool;
        Ref<LODPool>                 mLODPool;
        Ref<MeshPool>                mMeshPool;
        Ref<Renderer>                mRenderer;
        Ref<FreyaOptions>            mFreyaOptions;
        Ref<skr::Logger<LODService>> mLogger;
        Ref<skr::ServiceProvider>    mServiceProvider;

        // GPU buffers
        Ref<Buffer> mInstanceBuffer;    // LODInstanceData[] - read by compute
        Ref<Buffer> mDrawCommandBuffer; // DrawIndexedIndirectCommand[] -
                                        // written by compute, read by graphics
        Ref<Buffer> mDrawCountBuffer;   // uint32_t - atomic counter for visible
                                        // instance count
        Ref<Buffer> mTransformBuffer;   // mat4[] - transform matrices
        Ref<Buffer>
            mLODLevelsBuffer; // LODLevel[] - flat array of LOD level metadata

        // CPU mirrors for debugging/editor
        std::vector<LODInstanceData> mInstances;
        std::uint32_t                mInstanceCount = 0;
        std::uint32_t                mMaxInstances  = 65536;

        // Descriptor set
        vk::DescriptorPool      mDescriptorPool      = VK_NULL_HANDLE;
        vk::DescriptorSet       mLODDescriptorSet    = VK_NULL_HANDLE;
        vk::DescriptorSetLayout mDescriptorSetLayout = VK_NULL_HANDLE;

        // Compute pipeline
        vk::Pipeline       mLODComputePipeline       = VK_NULL_HANDLE;
        vk::PipelineLayout mLODComputePipelineLayout = VK_NULL_HANDLE;

        // Push constants
        LODPushConstants mPushConstants;

        // Dither texture
        Ref<Texture> mDitherTexture;
    };

} // namespace FREYA_NAMESPACE