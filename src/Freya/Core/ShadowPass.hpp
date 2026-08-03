#pragma once

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
     * render pass, a depth-only graphics pipeline (push-constant light
     * view-projection, no descriptor sets), one framebuffer per
     * cascade/spot layer and per point cube face, a host-visible
     * ShadowUniformBuffer, and a hardware comparison sampler used for
     * PCF-style shadow sampling by the lighting pass.
     */
    class ShadowPass
    {
      public:
        ShadowPass(
            const skr::Arc<Device>&             device,
            const skr::Arc<PhysicalDevice>&     physicalDevice,
            const skr::Arc<FreyaOptions>&       freyaOptions,
            vk::RenderPass                      renderPass,
            vk::PipelineLayout                  pipelineLayout,
            vk::Pipeline                        pipeline,
            vk::Image                           cascadeImage,
            vk::DeviceMemory                    cascadeMemory,
            vk::ImageView                       cascadeArrayView,
            const std::vector<vk::ImageView>&   cascadeLayerViews,
            const std::vector<vk::Framebuffer>& cascadeFramebuffers,
            vk::Image                           spotImage,
            vk::DeviceMemory                    spotMemory,
            vk::ImageView                       spotArrayView,
            const std::vector<vk::ImageView>&   spotLayerViews,
            const std::vector<vk::Framebuffer>& spotFramebuffers,
            vk::Image                           pointImage,
            vk::DeviceMemory                    pointMemory,
            vk::ImageView                       pointArrayView,
            const std::vector<vk::ImageView>&   pointFaceViews,
            const std::vector<vk::Framebuffer>& pointFramebuffers,
            const skr::Arc<Buffer>&             uniformBuffer,
            vk::Sampler                         compareSampler,
            vk::Sampler                         regularSampler,
            std::uint32_t                       cascadeCount,
            std::uint32_t                       maxSpotShadows,
            std::uint32_t                       maxPointShadows);

        ~ShadowPass();

        ShadowPass(const ShadowPass&)            = delete;
        ShadowPass& operator=(const ShadowPass&) = delete;

        /**
         * @brief Recomputes light view-projections and uploads the
         * ShadowUniformBuffer.
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
         */
        void Update(const LightService& lights,
                    const glm::mat4&    cameraView,
                    const glm::mat4&    cameraProj,
                    const glm::vec3&    cameraPos,
                    float               nearPlane,
                    float               drawDistance);

        /**
         * @brief Renders every active shadow target.
         *
         * For each cascade layer, active spot slot, and active point
         * light face: begins the depth-only render pass on the
         * corresponding framebuffer, binds the depth pipeline, sets the
         * viewport/scissor to the shadow map resolution, pushes the
         * light view-projection matrix, invokes `drawScene`, then ends
         * the render pass. `drawScene` is expected to bind vertex/index
         * buffers and issue draw calls only (pipeline and push constants
         * are already bound).
         *
         * @param commandPool Command pool with the currently recording
         *                    primary command buffer
         * @param drawScene   Callback that issues scene draw calls
         */
        void Render(const skr::Arc<CommandPool>& commandPool,
                    const std::function<void()>& drawScene) const;

        /**
         * @brief Returns the shadow uniform buffer (single copy, host
         * visible, updated every frame by Update()).
         */
        skr::Arc<Buffer> GetUniformBuffer() const { return mUniformBuffer; }

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
         * @brief Returns the regular (non-comparison) nearest sampler.
         */
        vk::Sampler GetSampler() const { return mSampler; }

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

      private:
        void computeCascades(const Light&     sun,
                             const glm::mat4& cameraView,
                             const glm::mat4& cameraProj,
                             float            nearPlane,
                             float            drawDistance);

        glm::mat4 computeSpotViewProj(const Light& light) const;

        glm::mat4 computePointFaceViewProj(
            const glm::vec3& position, float far, std::uint32_t face) const;

        void renderCascades(const skr::Arc<CommandPool>& commandPool,
                            const std::function<void()>& drawScene) const;

        void renderSpots(const skr::Arc<CommandPool>& commandPool,
                         const std::function<void()>& drawScene) const;

        void renderPoints(const skr::Arc<CommandPool>& commandPool,
                          const std::function<void()>& drawScene) const;

        skr::Arc<Device>         mDevice;
        skr::Arc<PhysicalDevice> mPhysicalDevice;
        skr::Arc<FreyaOptions>   mFreyaOptions;

        vk::RenderPass     mRenderPass;
        vk::PipelineLayout mPipelineLayout;
        vk::Pipeline       mPipeline;

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
        vk::Sampler mSampler;

        std::uint32_t mCascadeCount;
        std::uint32_t mMaxSpotShadows;
        std::uint32_t mMaxPointShadows;
        std::uint32_t mResolution;

        // CPU-side mirror of the last uploaded ShadowUniformBuffer, used by
        // Render() to know which light view-projection to push per target.
        ShadowUniformBuffer mShadowData {};
        std::uint32_t       mActiveSpotCount  = 0;
        std::uint32_t       mActivePointCount = 0;
    };

} // namespace FREYA_NAMESPACE
