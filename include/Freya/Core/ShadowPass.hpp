#pragma once

#include "Freya/Asset/BoneMatrixResources.hpp"
#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/LightService.hpp"
#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/Core/UniformBuffer.hpp"
#include "Freya/FreyaOptions.hpp"

#include <functional>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Depth-only shadow map pass (CSM cascades, spot, and point).
     *
     * Owns three depth image arrays (directional CSM cascades, spot
     * lights, and a cube-array for point lights), a single depth-only
     * render pass, hardware-depth and linear-depth pipelines, one
     * framebuffer per cascade/spot layer and per point cube face, a
     * host-visible ShadowUniformBuffer, and a comparison sampler.
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
            vk::PipelineLayout                   pipelineLayout,
            vk::Pipeline                         pipeline,
            vk::Pipeline                         pointPipeline,
            vk::Image                            cascadeImage,
            vk::DeviceMemory                     cascadeMemory,
            vk::ImageView                        cascadeArrayView,
            const std::vector<vk::ImageView>&    cascadeLayerViews,
            const std::vector<vk::Framebuffer>&  cascadeFramebuffers,
            vk::Image                            spotImage,
            vk::DeviceMemory                     spotMemory,
            vk::ImageView                        spotArrayView,
            const std::vector<vk::ImageView>&    spotLayerViews,
            const std::vector<vk::Framebuffer>&  spotFramebuffers,
            vk::Image                            pointImage,
            vk::DeviceMemory                     pointMemory,
            vk::ImageView                        pointArrayView,
            const std::vector<vk::ImageView>&    pointFaceViews,
            const std::vector<vk::Framebuffer>&  pointFramebuffers,
            const skr::Arc<Buffer>&              uniformBuffer,
            vk::Sampler                          compareSampler,
            std::uint32_t                        cascadeCount,
            std::uint32_t                        maxSpotShadows,
            std::uint32_t                        maxPointShadows);

        ~ShadowPass();

        ShadowPass(const ShadowPass&)            = delete;
        ShadowPass& operator=(const ShadowPass&) = delete;

        /**
         * @brief Takes ownership of GPU resources from `other`, destroying
         * the previous maps/pipeline on this instance. `other` is left empty
         * (safe to destroy). Used to recreate maps after quality changes
         * while keeping the same ShadowPass Arc alive for DI.
         */
        void StealResourcesFrom(ShadowPass& other);

        /**
         * @brief Recomputes light view-projections and uploads the
         * ShadowUniformBuffer for the given in-flight frame slot.
         *
         * Finds the first shadow-casting directional light and builds
         * practical-split CSM cascades fit to the camera frustum slices.
         * Collects up to `maxSpotShadows` shadow-casting spot lights and
         * `maxPointShadows` shadow-casting point lights.
         *
         * @param lights       Light service holding the active light list
         * @param cameraView   Current camera view matrix
         * @param cameraProj   Current camera projection matrix
         * @param cameraPos    Current camera world position (unused by CSM
         *                     fitting itself, kept for API symmetry / future
         *                     use by distance-based heuristics)
         * @param nearPlane    Camera near plane distance
         * @param drawDistance Camera far/draw distance
         * @param frameIndex   Swapchain/in-flight frame index for the UBO ring
         */
        void Update(const LightService& lights,
                    const glm::mat4&    cameraView,
                    const glm::mat4&    cameraProj,
                    const glm::vec3&    cameraPos,
                    float               nearPlane,
                    float               drawDistance,
                    std::uint32_t       frameIndex);

        /**
         * @brief Renders every active shadow target.
         *
         * For each cascade layer, every allocated spot slot, and every
         * allocated point cube face: begins the depth-only render pass
         * (clear → `SHADER_READ_ONLY_OPTIMAL` so unused layers are safe
         * to sample), and for active lights binds the depth pipeline,
         * sets viewport/scissor, pushes the light VP, and invokes
         * `drawScene`. `prepareCull` runs before each active view's
         * render pass so compute can fill the indirect buffer outside
         * the pass. `drawScene` should bind VB/IB and draw only.
         *
         * @param commandPool Command pool with the currently recording
         *                    primary command buffer
         * @param prepareCull Callback invoked with the light view-proj
         *                    before beginning the active view render pass
         * @param drawScene   Callback that issues scene draw calls
         */
        void Render(const skr::Arc<CommandPool>&                 commandPool,
                    const std::function<void(const glm::mat4&)>& prepareCull,
                    const std::function<void()>& drawScene) const;

        /**
         * @brief Returns the shadow uniform buffer (ring-buffered, host
         * visible, updated every frame by Update()).
         */
        skr::Arc<Buffer> GetUniformBuffer() const { return mUniformBuffer; }

        /**
         * @brief Byte offset of the ShadowUniformBuffer slot for `frameIndex`.
         */
        [[nodiscard]] std::uint64_t GetUniformBufferOffset(
            std::uint32_t frameIndex) const
        {
            return static_cast<std::uint64_t>(frameIndex) *
                   sizeof(ShadowUniformBuffer);
        }

        /**
         * @brief Returns the full cascade depth array view (2D array).
         */
        vk::ImageView GetCascadeView() const { return mCascadeArrayView; }

        /**
         * @brief Returns the full spot depth array view (2D array).
         */
        vk::ImageView GetSpotView() const { return mSpotArrayView; }

        /**
         * @brief Returns the point depth cube-array sampling view, or
         * null if point shadows are disabled.
         */
        vk::ImageView GetPointView() const { return mPointArrayView; }

        /**
         * @brief Returns the hardware comparison sampler used for PCF
         * shadow sampling.
         */
        vk::Sampler GetCompareSampler() const { return mCompareSampler; }

        /**
         * @brief Returns the number of configured CSM cascades.
         */
        std::uint32_t GetCascadeCount() const { return mCascadeCount; }

        /**
         * @brief Returns the maximum number of concurrent spot shadows.
         */
        std::uint32_t GetMaxSpotShadows() const { return mMaxSpotShadows; }

        /**
         * @brief Returns the maximum number of concurrent point shadows.
         */
        std::uint32_t GetMaxPointShadows() const { return mMaxPointShadows; }

        /**
         * @brief Returns true if spot shadow slots are configured.
         */
        bool HasSpotShadows() const { return mMaxSpotShadows > 0; }

        /**
         * @brief Returns true if point shadow slots are configured.
         */
        bool HasPointShadows() const { return mMaxPointShadows > 0; }

        /**
         * @brief Graphics layout (set 0 bones, set 1 bindless materials).
         *
         * Used as `drawPipelineLayoutOverride` so MDI can bind the texture
         * heap for Mask alpha testing in the shadow fragment shaders.
         */
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

        skr::Arc<Device>              mDevice;
        skr::Arc<PhysicalDevice>      mPhysicalDevice;
        skr::Arc<FreyaOptions>        mFreyaOptions;
        skr::Arc<BoneMatrixResources> mBoneResources;
        std::uint32_t                 mFrameIndex = 0;

        vk::RenderPass     mRenderPass;
        vk::PipelineLayout mPipelineLayout;
        vk::Pipeline       mPipeline;
        vk::Pipeline       mPointPipeline;

        // Directional CSM cascade resources (2D array).
        vk::Image                    mCascadeImage;
        vk::DeviceMemory             mCascadeMemory;
        vk::ImageView                mCascadeArrayView;
        std::vector<vk::ImageView>   mCascadeLayerViews;
        std::vector<vk::Framebuffer> mCascadeFramebuffers;

        // Spot light resources (2D array).
        vk::Image                    mSpotImage;
        vk::DeviceMemory             mSpotMemory;
        vk::ImageView                mSpotArrayView;
        std::vector<vk::ImageView>   mSpotLayerViews;
        std::vector<vk::Framebuffer> mSpotFramebuffers;

        // Point light resources (cube-compatible 2D array, 6 faces/light).
        vk::Image                    mPointImage;
        vk::DeviceMemory             mPointMemory;
        vk::ImageView                mPointArrayView;
        std::vector<vk::ImageView>   mPointFaceViews;
        std::vector<vk::Framebuffer> mPointFramebuffers;

        skr::Arc<Buffer> mUniformBuffer;

        vk::Sampler mCompareSampler;

        std::uint32_t mCascadeCount;
        std::uint32_t mMaxSpotShadows;
        std::uint32_t mMaxPointShadows;
        std::uint32_t mResolution;
        /// Spot map extent (1 when spots disabled — tiny descriptor stub).
        std::uint32_t mSpotResolution = 1;
        /// Point cube face extent (1 when points disabled).
        std::uint32_t mPointResolution = 1;

        // CPU-side mirror of the last uploaded ShadowUniformBuffer, used by
        // Render() to know which light view-projection to push per target.
        ShadowUniformBuffer mShadowData {};
        bool                mHasDirectionalShadow = false;
        std::uint32_t       mActiveSpotCount      = 0;
        std::uint32_t       mActivePointCount     = 0;
    };

} // namespace FREYA_NAMESPACE
