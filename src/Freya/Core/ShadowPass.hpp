#pragma once

#include "Freya/Asset/BoneMatrixResources.hpp"
#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/LightService.hpp"
#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/Core/UniformBuffer.hpp"
#include "Freya/FreyaOptions.hpp"

#include <array>
#include <functional>
#include <vector>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Depth-only shadow map pass (CSM cascades, spot, and point).
     *
     * Directional CSM and point cubes use VK multiview (one render pass /
     * one draw for all cascade layers or cube faces). Spots stay
     * per-layer.
     */
    class ShadowPass
    {
      public:
        ShadowPass(
            const skr::Arc<Device>&              device,
            const skr::Arc<PhysicalDevice>&      physicalDevice,
            const skr::Arc<FreyaOptions>&        freyaOptions,
            const skr::Arc<BoneMatrixResources>& boneResources,
            vk::RenderPass                       renderPass,
            vk::RenderPass                       cascadeRenderPass,
            vk::RenderPass                       pointRenderPass,
            vk::PipelineLayout                   pipelineLayout,
            vk::Pipeline                         pipeline,
            vk::Pipeline                         cascadePipeline,
            vk::Pipeline                         pointPipeline,
            vk::Image                            cascadeImage,
            vk::DeviceMemory                     cascadeMemory,
            vk::ImageView                        cascadeArrayView,
            const std::vector<vk::ImageView>&    cascadeLayerViews,
            vk::Framebuffer                      cascadeFramebuffer,
            const std::vector<vk::Framebuffer>&  spotFramebuffers,
            vk::Image                            spotImage,
            vk::DeviceMemory                     spotMemory,
            vk::ImageView                        spotArrayView,
            const std::vector<vk::ImageView>&    spotLayerViews,
            vk::Image                            pointImage,
            vk::DeviceMemory                     pointMemory,
            vk::ImageView                        pointArrayView,
            const std::vector<vk::ImageView>&    pointSlotViews,
            const std::vector<vk::Framebuffer>&  pointFramebuffers,
            const skr::Arc<Buffer>&              uniformBuffer,
            vk::Sampler                          compareSampler,
            vk::DescriptorSetLayout              shadowUboSetLayout,
            vk::DescriptorPool                   shadowUboPool,
            std::vector<vk::DescriptorSet>       shadowUboSets,
            std::uint32_t                        cascadeCount,
            std::uint32_t                        maxSpotShadows,
            std::uint32_t                        maxPointShadows);

        ~ShadowPass();

        ShadowPass(const ShadowPass&)            = delete;
        ShadowPass& operator=(const ShadowPass&) = delete;

        void StealResourcesFrom(ShadowPass& other);

        void Update(const LightService& lights,
                    const glm::mat4&    cameraView,
                    const glm::mat4&    cameraProj,
                    const glm::vec3&    cameraPos,
                    float               nearPlane,
                    float               drawDistance,
                    std::uint32_t       frameIndex);

        void Render(const skr::Arc<CommandPool>&                 commandPool,
                    const std::function<void(const glm::mat4&)>& prepareCull,
                    const std::function<void()>& drawScene) const;

        skr::Arc<Buffer> GetUniformBuffer() const { return mUniformBuffer; }

        [[nodiscard]] std::uint64_t GetUniformBufferOffset(
            std::uint32_t frameIndex) const
        {
            return static_cast<std::uint64_t>(frameIndex) *
                   sizeof(ShadowUniformBuffer);
        }

        vk::ImageView GetCascadeView() const { return mCascadeArrayView; }
        vk::ImageView GetSpotView() const { return mSpotArrayView; }
        vk::ImageView GetPointView() const { return mPointArrayView; }
        vk::Sampler   GetCompareSampler() const { return mCompareSampler; }

        std::uint32_t GetCascadeCount() const { return mCascadeCount; }
        std::uint32_t GetMaxSpotShadows() const { return mMaxSpotShadows; }
        std::uint32_t GetMaxPointShadows() const { return mMaxPointShadows; }
        bool          HasSpotShadows() const { return mMaxSpotShadows > 0; }
        bool          HasPointShadows() const { return mMaxPointShadows > 0; }

        [[nodiscard]] vk::PipelineLayout GetPipelineLayout() const
        {
            return mPipelineLayout;
        }

      private:
        void computeCascades(const Light&     sun,
                             const glm::mat4& cameraView,
                             const glm::mat4& cameraProj,
                             float            nearPlane,
                             float            drawDistance);

        glm::mat4 computeSpotViewProj(const Light& light) const;

        glm::mat4 computePointFaceViewProj(
            const glm::vec3& position, float far, std::uint32_t face) const;

        glm::mat4 computePointCullViewProj(const glm::vec3& position,
                                           float            far) const;

        void renderCascades(
            const skr::Arc<CommandPool>&                 commandPool,
            const std::function<void(const glm::mat4&)>& prepareCull,
            const std::function<void()>&                 drawScene) const;

        void renderSpots(
            const skr::Arc<CommandPool>&                 commandPool,
            const std::function<void(const glm::mat4&)>& prepareCull,
            const std::function<void()>&                 drawScene) const;

        void renderPoints(
            const skr::Arc<CommandPool>&                 commandPool,
            const std::function<void(const glm::mat4&)>& prepareCull,
            const std::function<void()>&                 drawScene) const;

        void destroyGpuResources();
        void bindBoneDescriptorSet(vk::CommandBuffer commandBuffer) const;
        void bindShadowUboSet(vk::CommandBuffer commandBuffer) const;

        skr::Arc<Device>              mDevice;
        skr::Arc<PhysicalDevice>      mPhysicalDevice;
        skr::Arc<FreyaOptions>        mFreyaOptions;
        skr::Arc<BoneMatrixResources> mBoneResources;
        std::uint32_t                 mFrameIndex = 0;

        vk::RenderPass     mRenderPass;        ///< Spot (no multiview)
        vk::RenderPass     mCascadeRenderPass; ///< CSM multiview
        vk::RenderPass     mPointRenderPass;   ///< Point cube multiview
        vk::PipelineLayout mPipelineLayout;
        vk::Pipeline       mPipeline;        ///< Spot (HW depth)
        vk::Pipeline       mCascadePipeline; ///< CSM multiview
        vk::Pipeline       mPointPipeline;   ///< Point cube multiview

        vk::Image                    mCascadeImage;
        vk::DeviceMemory             mCascadeMemory;
        vk::ImageView                mCascadeArrayView;
        std::vector<vk::ImageView>   mCascadeLayerViews;
        vk::Framebuffer              mCascadeFramebuffer = {};

        vk::Image                    mSpotImage;
        vk::DeviceMemory             mSpotMemory;
        vk::ImageView                mSpotArrayView;
        std::vector<vk::ImageView>   mSpotLayerViews;
        std::vector<vk::Framebuffer> mSpotFramebuffers;

        vk::Image                    mPointImage;
        vk::DeviceMemory             mPointMemory;
        vk::ImageView                mPointArrayView;
        std::vector<vk::ImageView>   mPointSlotViews;
        std::vector<vk::Framebuffer> mPointFramebuffers;

        skr::Arc<Buffer> mUniformBuffer;
        vk::Sampler      mCompareSampler;

        vk::DescriptorSetLayout        mShadowUboSetLayout = {};
        vk::DescriptorPool             mShadowUboPool      = {};
        std::vector<vk::DescriptorSet> mShadowUboSets;

        std::uint32_t mCascadeCount;
        std::uint32_t mMaxSpotShadows;
        std::uint32_t mMaxPointShadows;
        std::uint32_t mResolution;
        std::uint32_t mSpotResolution  = 1;
        std::uint32_t mPointResolution = 1;

        ShadowUniformBuffer mShadowData {};
        glm::mat4           mCascadeCullViewProj { 1.0f };
        bool                mHasDirectionalShadow = false;
        std::uint32_t       mActiveSpotCount      = 0;
        std::uint32_t       mActivePointCount     = 0;

        /// Temporal CSM: skip redraw when camera/sun stable.
        bool          mCascadesNeedRedraw = true;
        std::uint32_t mCascadeUpdateAge   = 0;
        glm::mat4     mLastCameraView { 0.0f };
        glm::mat4     mLastCameraProj { 0.0f };
        glm::vec3     mLastSunDir { 0.0f };
        bool          mHasLastCascadeMotion = false;

        /// Temporal point cubes: skip when light pos/range stable.
        std::array<bool, MAX_POINT_SHADOWS>      mPointNeedRedraw {};
        mutable std::array<bool, MAX_POINT_SHADOWS> mPointNeedClear {};
        mutable std::array<bool, MAX_POINT_SHADOWS> mPointHasContent {};
        std::array<bool, MAX_POINT_SHADOWS>      mPointHasLast {};
        std::array<glm::vec4, MAX_POINT_SHADOWS>  mLastPointPosFar {};
        std::array<std::uint32_t, MAX_POINT_SHADOWS> mPointUpdateAge {};
    };

} // namespace FREYA_NAMESPACE
