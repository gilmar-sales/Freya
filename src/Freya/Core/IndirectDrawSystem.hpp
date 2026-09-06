#pragma once

#include "Freya/Asset/GpuScene.hpp"
#include "Freya/Asset/InstanceTransform.hpp"
#include "Freya/Asset/MaterialDescriptorResources.hpp"
#include "Freya/Asset/MaterialPool.hpp"
#include "Freya/Asset/MeshPool.hpp"
#include "Freya/Asset/SceneBvh.hpp"
#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/HiZPyramid.hpp"
#include "Freya/Core/Image.hpp"

#include <array>
#include <limits>
#include <span>
#include <vector>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief GPU-driven scene: Approach B MDI + frustum/Hi-Z cull + LOD.
     *
     * Optional hierarchical BVH pre-filter (level-synchronous indirect
     * dispatches) produces a candidate instance list; flat CullFrustum then
     * compacta uma DrawIndexedIndirectCommand por instância visível
     * (LOD selecionado) e a CPU emite um único drawIndexedIndirectCount.
     */
    class IndirectDrawSystem
    {
      public:
        IndirectDrawSystem(
            const skr::Arc<Device>&                      device,
            const skr::Arc<CommandPool>&                 commandPool,
            const skr::Arc<MeshPool>&                    meshPool,
            const skr::Arc<MaterialDescriptorResources>& materials,
            const skr::Arc<MaterialPool>&                materialPool,
            std::uint32_t                                frameCount,
            vk::Pipeline                                 cullPipeline,
            vk::PipelineLayout                           cullPipelineLayout,
            vk::DescriptorSetLayout                      cullSetLayout,
            vk::DescriptorPool                           cullDescriptorPool,
            std::vector<vk::DescriptorSet>
                                    cullDescriptorSets,
            vk::Pipeline            bvhCullPipeline,
            vk::PipelineLayout      bvhCullPipelineLayout,
            vk::DescriptorSetLayout bvhCullSetLayout,
            vk::DescriptorPool      bvhCullDescriptorPool,
            std::vector<vk::DescriptorSet>
                                    bvhCullDescriptorSets,
            vk::Pipeline            preparePipeline,
            vk::PipelineLayout      preparePipelineLayout,
            vk::DescriptorSetLayout prepareSetLayout,
            vk::DescriptorPool      prepareDescriptorPool,
            std::vector<vk::DescriptorSet>
                prepareDescriptorSets,
            skr::Arc<HiZPyramid>
                hiz,
            skr::Arc<Image>
                          hizFallbackImage,
            bool          enableHierarchicalCulling,
            std::uint32_t hierarchicalCullMinInstances);

        ~IndirectDrawSystem();

        void UploadSceneInstances(std::span<const SceneInstanceUpload> uploads,
                                  std::uint32_t frameIndex);

        void SyncMeshInfo();

        void SetCullView(const glm::vec3& cameraPos, vk::Extent2D screenSize);

        void ResizeHiZ(vk::Extent2D extent);

        void BuildHiZ(const skr::Arc<Image>& depthImage, bool reverseZ);

        /**
         * @brief Reset drawCount, dispatch cull+compact+LOD.
         *
         * Must be called outside of a render pass.
         */
        void DispatchCull(const glm::mat4& viewProj, CullMode mode,
                          bool          reverseZ        = false,
                          std::uint32_t techniqueFilter = kTechniqueFilterAll);

        void ExecuteDraws(bool               bindMaterials,
                          vk::PipelineLayout pipelineLayout,
                          std::uint32_t techniqueFilter = kTechniqueFilterAll);

        [[nodiscard]] std::uint32_t GetInstanceCount() const
        {
            return mInstanceCount;
        }

        [[nodiscard]] std::uint32_t GetBatchCount() const
        {
            return mInstanceCount;
        }

        [[nodiscard]] bool HasScene() const { return mInstanceCount > 0; }

        /// Bit i set ⇒ at least one opaque instance uses technique i.
        [[nodiscard]] std::uint32_t UsedTechniqueMask() const
        {
            return mUsedTechniqueMask;
        }

      private:
        class EntityModelMap
        {
          public:
            static constexpr std::uint32_t kEmpty =
                std::numeric_limits<std::uint32_t>::max();

            void clear()
            {
                ++mGeneration;
                mCount = 0;
                if (mGeneration == 0)
                {
                    mGeneration = 1;
                    for (auto& slot : mSlots)
                    {
                        slot.entityId   = kEmpty;
                        slot.generation = 0;
                    }
                }
            }

            void reserve(const std::size_t n)
            {
                std::size_t need = 16;
                while (need < n * 2)
                    need *= 2;
                if (need > mSlots.size())
                    rehash(need);
            }

            void insert(const std::uint32_t entityId, const glm::mat4& model)
            {
                if (entityId == kEmpty)
                    return;

                if (mSlots.empty() || mCount * 2 >= mSlots.size())
                    reserve(std::max<std::size_t>(mCount + 1, 16));

                const auto mask = mSlots.size() - 1;
                auto       i    = hash(entityId) & mask;
                for (;;)
                {
                    auto& slot = mSlots[i];
                    if (slot.entityId == kEmpty ||
                        slot.generation != mGeneration)
                    {
                        slot.entityId   = entityId;
                        slot.generation = mGeneration;
                        slot.model      = model;
                        ++mCount;
                        return;
                    }
                    if (slot.entityId == entityId)
                    {
                        slot.model = model;
                        return;
                    }
                    i = (i + 1) & mask;
                }
            }

            [[nodiscard]] const glm::mat4* find(
                const std::uint32_t entityId) const
            {
                if (mSlots.empty() || entityId == kEmpty)
                    return nullptr;

                const auto mask = mSlots.size() - 1;
                auto       i    = hash(entityId) & mask;
                for (;;)
                {
                    const auto& slot = mSlots[i];
                    if (slot.entityId == kEmpty ||
                        slot.generation != mGeneration)
                        return nullptr;
                    if (slot.entityId == entityId)
                        return &slot.model;
                    i = (i + 1) & mask;
                }
            }

          private:
            struct Slot
            {
                std::uint32_t entityId   = kEmpty;
                std::uint32_t generation = 0;
                glm::mat4     model { 1.0f };
            };

            static std::uint32_t hash(std::uint32_t x)
            {
                x ^= x >> 16;
                x *= 0x7feb352du;
                x ^= x >> 15;
                x *= 0x846ca68bu;
                x ^= x >> 16;
                return x;
            }

            void rehash(const std::size_t newCap)
            {
                std::vector<Slot> old = std::move(mSlots);
                mSlots.assign(newCap, Slot {});
                const auto  mask = newCap - 1;
                std::size_t live = 0;

                for (const auto& slot : old)
                {
                    if (slot.entityId == kEmpty ||
                        slot.generation != mGeneration)
                        continue;

                    auto i = hash(slot.entityId) & mask;
                    for (;;)
                    {
                        auto& dst = mSlots[i];
                        if (dst.entityId == kEmpty ||
                            dst.generation != mGeneration)
                        {
                            dst = slot;
                            ++live;
                            break;
                        }
                        i = (i + 1) & mask;
                    }
                }

                mCount = live;
            }

            std::vector<Slot> mSlots;
            std::uint32_t     mGeneration = 1;
            std::size_t       mCount      = 0;
        };

        struct DrawListResources
        {
            skr::Arc<Buffer> compactTransforms;
            skr::Arc<Buffer> indirect;
            skr::Arc<Buffer> drawCount;
        };

        struct FrameResources
        {
            skr::Arc<Buffer>  sceneInstances;
            skr::Arc<Buffer>  sourceTransforms;
            DrawListResources main;
            std::array<DrawListResources, kMaxMaterialTechniques> techniques {};
            std::uint32_t                                         capacity = 0;

            // Hierarchical cull (shared across technique draw lists).
            skr::Arc<Buffer> bvhNodes;
            skr::Arc<Buffer> bvhLeafInstances;
            skr::Arc<Buffer> bvhQueueA;
            skr::Arc<Buffer> bvhQueueB;
            skr::Arc<Buffer> bvhCounterA;
            skr::Arc<Buffer> bvhCounterB;
            skr::Arc<Buffer> bvhDispatchArgsA;
            skr::Arc<Buffer> bvhDispatchArgsB;
            skr::Arc<Buffer> candidateInstances;
            skr::Arc<Buffer> candidateCount;
            skr::Arc<Buffer> cullDispatchArgs;
            std::uint32_t    bvhNodeCapacity   = 0;
            std::uint32_t    bvhQueueCapacity  = 0;
            std::uint32_t    candidateCapacity = 0;
        };

        void ensureCapacity(std::uint32_t instanceCount);
        void ensureCapacityForFrame(std::uint32_t frameIndex,
                                    std::uint32_t instanceCount);
        void ensureBvhCapacity(std::uint32_t frameIndex,
                               std::uint32_t nodeCount,
                               std::uint32_t instanceCount);
        void updateCullDescriptors(std::uint32_t frameIndex);
        void updateBvhDescriptors(std::uint32_t frameIndex);
        void bumpCullDescVersion();
        void refreshCullDescriptorsIfNeeded();
        void uploadFrameBuffers();
        void uploadBvhBuffers();
        void zeroDrawCount(std::uint32_t techniqueFilter);
        void rebuildOrRefitBvh();
        void dispatchBvhTraversal(const glm::mat4& viewProj, bool reverseZ,
                                  bool hizEnabled);
        [[nodiscard]] bool shouldUseHierarchicalCull() const;

        [[nodiscard]] FrameResources&    currentFrame();
        [[nodiscard]] DrawListResources& drawListFor(
            FrameResources& frame, std::uint32_t techniqueFilter);
        [[nodiscard]] vk::DescriptorSet cullSetFor(
            std::uint32_t techniqueFilter) const;

        static constexpr std::uint32_t kCullSetsPerFrame =
            1u + kMaxMaterialTechniques;
        /// Two ping-pong BVH sets + one prepare-for-cull set per frame.
        static constexpr std::uint32_t kBvhSetsPerFrame     = 2u;
        static constexpr std::uint32_t kPrepareSetsPerFrame = 3u;

        skr::Arc<Device>                      mDevice;
        skr::Arc<CommandPool>                 mCommandPool;
        skr::Arc<MeshPool>                    mMeshPool;
        skr::Arc<MaterialDescriptorResources> mMaterials;
        skr::Arc<MaterialPool>                mMaterialPool;

        std::uint32_t mFrameCount          = 1;
        std::uint32_t mFrameIndex          = 0;
        std::uint64_t mFrameSerial         = 0;
        std::uint64_t mHiZMotionSerial     = ~std::uint64_t(0);
        bool          mHiZSafeForFrame     = false;
        bool          mHasLastCullViewProj = false;
        glm::mat4     mLastCullViewProj { 1.0f };

        vk::Pipeline                   mCullPipeline;
        vk::PipelineLayout             mCullPipelineLayout;
        vk::DescriptorSetLayout        mCullSetLayout;
        vk::DescriptorPool             mCullDescriptorPool;
        std::vector<vk::DescriptorSet> mCullDescriptorSets;

        vk::Pipeline                   mBvhCullPipeline;
        vk::PipelineLayout             mBvhCullPipelineLayout;
        vk::DescriptorSetLayout        mBvhCullSetLayout;
        vk::DescriptorPool             mBvhCullDescriptorPool;
        std::vector<vk::DescriptorSet> mBvhCullDescriptorSets;

        vk::Pipeline                   mPreparePipeline;
        vk::PipelineLayout             mPreparePipelineLayout;
        vk::DescriptorSetLayout        mPrepareSetLayout;
        vk::DescriptorPool             mPrepareDescriptorPool;
        std::vector<vk::DescriptorSet> mPrepareDescriptorSets;

        skr::Arc<HiZPyramid> mHiZ;
        skr::Arc<Image>      mHizFallbackImage;

        skr::Arc<Buffer>            mMeshInfoBuffer;
        skr::Arc<Buffer>            mMeshLodBuffer;
        std::vector<FrameResources> mFrames;

        std::vector<MeshInfo>          mMeshInfos;
        std::vector<MeshLodInfo>       mMeshLods;
        std::vector<SceneInstance>     mSceneInstances;
        std::vector<InstanceTransform> mInstanceTransforms;
        std::vector<InstanceTransform> mPrevTransforms;
        EntityModelMap                 mPrevModelByEntity;

        SceneBvh                  mSceneBvh;
        std::vector<InstanceAabb> mInstanceAabbs;
        bool                      mEnableHierarchicalCulling = false;
        std::uint32_t             mHierarchicalCullMinInstances =
            kHierarchicalCullMinInstances;
        bool mBvhGpuDirty = true;

        std::uint32_t mInstanceCount     = 0;
        std::uint32_t mMeshInfoCapacity  = 0;
        std::uint32_t mMeshLodCapacity   = 0;
        bool          mMeshInfoDirty     = true;
        std::uint32_t mUsedTechniqueMask = 1u; // technique 0 always considered

        // Cull descriptor updates are unsafe once the set is bound this frame.
        // bumpCullDescVersion must not clear mCullDescRefreshedThisFrame.
        std::uint32_t              mCullDescVersion = 1;
        std::vector<std::uint32_t> mFrameCullDescVersion;
        bool                       mCullDescRefreshedThisFrame = false;

        glm::vec3    mCameraPos { 0.0f };
        vk::Extent2D mScreenSize { 1, 1 };
    };

} // namespace FREYA_NAMESPACE
