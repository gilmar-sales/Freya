#pragma once

#include "Freya/Asset/CullFrameDump.hpp"
#include "Freya/Asset/GpuScene.hpp"
#include "Freya/Asset/InstanceTransform.hpp"
#include "Freya/Asset/MaterialDescriptorResources.hpp"
#include "Freya/Asset/MeshPool.hpp"
#include "Freya/Asset/SceneInstanceUpload.hpp"
#include "Freya/Asset/SceneTransform.hpp"
#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/HiZPyramid.hpp"
#include "Freya/Core/Image.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <shared_mutex>
#include <span>
#include <vector>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief GPU-driven scene: TRS expand + Approach B MDI + frustum/Hi-Z +
     * LOD.
     *
     * Frame upload: BeginSceneInstances → Reserve → Upload (any thread) →
     * EndSceneInstances. ExpandTransforms fills model/prevModel before cull.
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
                                    cullDescriptorSets,
            vk::Pipeline            expandPipeline,
            vk::PipelineLayout      expandPipelineLayout,
            vk::DescriptorSetLayout expandSetLayout,
            vk::DescriptorPool      expandDescriptorPool,
            std::vector<vk::DescriptorSet>
                expandDescriptorSets,
            skr::Arc<HiZPyramid>
                hiz,
            skr::Arc<Image>
                hizFallbackImage);

        ~IndirectDrawSystem();

        void BeginSceneInstances();
        void ReserveSceneInstances(std::uint32_t count);
        void UploadSceneInstances(std::span<const SceneInstanceUpload> uploads);
        void EndSceneInstances(std::uint32_t frameIndex);

        void CommitSceneFrame(std::uint32_t frameIndex);

        void SyncMeshInfo();

        void SetCullView(const glm::vec3& cameraPos, vk::Extent2D screenSize);

        void ResizeHiZ(vk::Extent2D extent);

        void BuildHiZ(const skr::Arc<Image>& depthImage, bool reverseZ);

        void DispatchCull(const glm::mat4& viewProj, CullMode mode,
                          bool          reverseZ        = false,
                          std::uint32_t techniqueFilter = kTechniqueFilterAll);

        void DispatchCullExact(const CullPushConstants& pc);

        void ExecuteDraws(bool               bindMaterials,
                          vk::PipelineLayout pipelineLayout,
                          std::uint32_t techniqueFilter = kTechniqueFilterAll);

        void CaptureCullInputs(CullFrameSnapshot& out) const;

        bool ReadbackCullOutputs(std::uint32_t              frameIndex,
                                 std::uint32_t              techniqueFilter,
                                 std::uint32_t&             outDrawCount,
                                 std::vector<CullSurvivor>& outSurvivors);

        bool ReadbackCullOutputsAggregated(
            std::uint32_t              frameIndex,
            std::uint32_t&             outDrawCount,
            std::vector<CullSurvivor>& outSurvivors);

        bool CaptureHiZ(CullHiZDump& out);

        bool UploadHiZFromDump(const CullHiZDump& dump);

        [[nodiscard]] const CullPushConstants& GetLastCullPushConstants() const
        {
            return mLastCullPushConstants;
        }

        [[nodiscard]] std::uint32_t GetInstanceCount() const
        {
            return mInstanceCount;
        }

        [[nodiscard]] std::uint32_t GetBatchCount() const
        {
            return mInstanceCount;
        }

        [[nodiscard]] bool HasScene() const { return mInstanceCount > 0; }

        [[nodiscard]] std::uint32_t UsedTechniqueMask() const
        {
            return mUsedTechniqueMask;
        }

      private:
        struct ExpandPushConstants
        {
            std::uint32_t instanceCount = 0;
            std::uint32_t prevCount     = 0;
            std::uint32_t _pad0         = 0;
            std::uint32_t _pad1         = 0;
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
            skr::Arc<Buffer>  sourceTransformsTRS;
            DrawListResources main;
            std::array<DrawListResources, kMaxMaterialTechniques> techniques {};
            std::uint32_t                                         capacity = 0;
        };

        void ensureCapacity(std::uint32_t instanceCount);
        void ensureCapacityForFrame(std::uint32_t frameIndex,
                                    std::uint32_t instanceCount);
        void ensurePrevCapacity(std::uint32_t instanceCount);
        void updateCullDescriptors(std::uint32_t frameIndex);
        void updateExpandDescriptors(std::uint32_t frameIndex);
        void bumpCullDescVersion();
        void refreshCullDescriptorsIfNeeded();
        void uploadFrameBuffers();
        void zeroDrawCount(std::uint32_t techniqueFilter);
        void recordDispatchCull(const CullPushConstants& pc);
        void ensureExpandedThisFrame();
        void beginFrameUpload(std::uint32_t frameIndex);
        void stampSceneVersion();
        void finalizeStagingToHost();

        [[nodiscard]] FrameResources&    currentFrame();
        [[nodiscard]] DrawListResources& drawListFor(
            FrameResources& frame, std::uint32_t techniqueFilter);
        [[nodiscard]] vk::DescriptorSet cullSetFor(
            std::uint32_t techniqueFilter) const;

        static constexpr std::uint32_t kCullSetsPerFrame =
            1u + kMaxMaterialTechniques;

        skr::Arc<Device>                      mDevice;
        skr::Arc<CommandPool>                 mCommandPool;
        skr::Arc<MeshPool>                    mMeshPool;
        skr::Arc<MaterialDescriptorResources> mMaterials;

        std::uint32_t     mFrameCount          = 1;
        std::uint32_t     mFrameIndex          = 0;
        std::uint64_t     mFrameSerial         = 0;
        std::uint64_t     mHiZMotionSerial     = ~std::uint64_t(0);
        bool              mHiZSafeForFrame     = false;
        bool              mHasLastCullViewProj = false;
        glm::mat4         mLastCullViewProj { 1.0f };
        CullPushConstants mLastCullPushConstants {};

        vk::Pipeline                   mCullPipeline;
        vk::PipelineLayout             mCullPipelineLayout;
        vk::DescriptorSetLayout        mCullSetLayout;
        vk::DescriptorPool             mCullDescriptorPool;
        std::vector<vk::DescriptorSet> mCullDescriptorSets;

        vk::Pipeline                   mExpandPipeline;
        vk::PipelineLayout             mExpandPipelineLayout;
        vk::DescriptorSetLayout        mExpandSetLayout;
        vk::DescriptorPool             mExpandDescriptorPool;
        std::vector<vk::DescriptorSet> mExpandDescriptorSets;

        skr::Arc<HiZPyramid> mHiZ;
        skr::Arc<Image>      mHizFallbackImage;

        skr::Arc<Buffer>            mMeshInfoBuffer;
        skr::Arc<Buffer>            mMeshLodBuffer;
        skr::Arc<Buffer>            mPrevSourceTransforms;
        std::uint32_t               mPrevCapacity      = 0;
        std::uint32_t               mPrevInstanceCount = 0;
        std::vector<FrameResources> mFrames;

        std::vector<MeshInfo>          mMeshInfos;
        std::vector<MeshLodInfo>       mMeshLods;
        std::vector<SceneInstance>     mSceneInstances;
        std::vector<InstanceTransform> mInstanceTransforms;
        std::vector<SceneTransform>    mSceneTransforms;

        std::vector<SceneInstanceUpload> mStaging;
        std::atomic<std::uint32_t>       mStagingCount { 0 };
        std::shared_mutex                mStagingMutex;
        bool                             mStagingOpen = false;

        std::uint32_t mInstanceCount     = 0;
        std::uint32_t mMeshInfoCapacity  = 0;
        std::uint32_t mMeshLodCapacity   = 0;
        bool          mMeshInfoDirty     = true;
        std::uint32_t mUsedTechniqueMask = 1u;
        bool          mExpandedThisFrame = false;

        std::uint64_t              mSceneVersion = 0;
        std::vector<std::uint64_t> mFrameSceneVersion;

        std::uint32_t              mCullDescVersion = 1;
        std::vector<std::uint32_t> mFrameCullDescVersion;
        bool                       mCullDescRefreshedThisFrame = false;

        glm::vec3    mCameraPos { 0.0f };
        vk::Extent2D mScreenSize { 1, 1 };
    };

} // namespace FREYA_NAMESPACE
