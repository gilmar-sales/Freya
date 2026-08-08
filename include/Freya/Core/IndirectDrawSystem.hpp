#pragma once

#include "Freya/Asset/GpuScene.hpp"
#include "Freya/Asset/InstanceTransform.hpp"
#include "Freya/Asset/MaterialDescriptorResources.hpp"
#include "Freya/Asset/MeshPool.hpp"
#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"

#include <limits>
#include <span>
#include <unordered_map>
#include <vector>

namespace FREYA_NAMESPACE
{
    /**
     * @brief GPU-driven scene: batched MDI, frustum cull + compact, draw.
     *
     * Contiguous uploads with the same meshId become one indirect command.
     * CullFrustum.comp (approach A) frustum-tests each instance and
     * atomic-compacts survivors into [firstInstance, firstInstance+visible)
     * of a compact transform buffer used for drawing.
     *
     * TAA history: `prevModel` is resolved by `entityId` (no full-vector
     * copy). Prefer uploading sorted by `(meshId, entityId)`.
     */
    class IndirectDrawSystem
    {
      public:
        IndirectDrawSystem(
            const skr::Arc<Device>&                      device,
            const skr::Arc<CommandPool>&                 commandPool,
            const skr::Arc<MeshPool>&                    meshPool,
            const skr::Arc<MaterialDescriptorResources>& materials,
            std::uint32_t                                frameCount,
            vk::Pipeline                                 cullPipeline,
            vk::PipelineLayout                           cullPipelineLayout,
            vk::DescriptorSetLayout                      cullSetLayout,
            vk::DescriptorPool                           cullDescriptorPool,
            std::vector<vk::DescriptorSet>
                cullDescriptorSets);

        ~IndirectDrawSystem();

        void UploadSceneInstances(std::span<const SceneInstanceUpload> uploads,
                                  std::uint32_t frameIndex);

        void SyncMeshInfo();

        /**
         * @brief Reset batch instanceCounts, dispatch cull+compact.
         *
         * Must be called outside of a render pass.
         */
        void DispatchCull(const glm::mat4& viewProj, CullMode mode,
                          bool reverseZ = false);

        void ExecuteDraws(bool               bindMaterials,
                          vk::PipelineLayout pipelineLayout);

        [[nodiscard]] std::uint32_t GetInstanceCount() const
        {
            return mInstanceCount;
        }

        [[nodiscard]] std::uint32_t GetBatchCount() const
        {
            return static_cast<std::uint32_t>(mIndirectCommands.size());
        }

        [[nodiscard]] bool HasScene() const { return mInstanceCount > 0; }

      private:
        struct FrameResources
        {
            skr::Arc<Buffer> sceneInstances;
            skr::Arc<Buffer> batchIds;
            skr::Arc<Buffer> sourceTransforms;
            skr::Arc<Buffer> compactTransforms;
            skr::Arc<Buffer> indirect;
            std::uint32_t    capacity = 0;
        };

        struct ChunkRun
        {
            std::uint32_t vertexBufferIndex = 0;
            std::uint32_t indexBufferIndex  = 0;
            std::uint32_t firstDraw         = 0;
            std::uint32_t drawCount         = 0;
        };

        void ensureCapacity(std::uint32_t instanceCount);
        void updateCullDescriptors(std::uint32_t frameIndex);
        void buildBatches();
        void uploadFrameBuffers();
        void uploadIndirectZeros();

        [[nodiscard]] FrameResources& currentFrame();

        skr::Arc<Device>                      mDevice;
        skr::Arc<CommandPool>                 mCommandPool;
        skr::Arc<MeshPool>                    mMeshPool;
        skr::Arc<MaterialDescriptorResources> mMaterials;

        std::uint32_t mFrameCount = 1;
        std::uint32_t mFrameIndex = 0;

        vk::Pipeline                   mCullPipeline;
        vk::PipelineLayout             mCullPipelineLayout;
        vk::DescriptorSetLayout        mCullSetLayout;
        vk::DescriptorPool             mCullDescriptorPool;
        std::vector<vk::DescriptorSet> mCullDescriptorSets;

        skr::Arc<Buffer>            mMeshInfoBuffer;
        std::vector<FrameResources> mFrames;

        std::vector<MeshInfo>                        mMeshInfos;
        std::vector<SceneInstance>                   mSceneInstances;
        std::vector<InstanceTransform>               mInstanceTransforms;
        std::vector<InstanceTransform>               mPrevTransforms;
        std::unordered_map<std::uint32_t, glm::mat4> mPrevModelByEntity;
        std::vector<std::uint32_t>                   mInstanceBatchIds;
        std::vector<vk::DrawIndexedIndirectCommand>  mIndirectCommands;
        std::vector<ChunkRun>                        mChunkRuns;

        std::uint32_t mInstanceCount    = 0;
        std::uint32_t mMeshInfoCapacity = 0;
        bool          mMeshInfoDirty    = true;
    };

} // namespace FREYA_NAMESPACE
