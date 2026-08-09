#pragma once

#include "Freya/Asset/BoneMatrixResources.hpp"
#include "Freya/Asset/GpuScene.hpp"
#include "Freya/Asset/MaterialPool.hpp"
#include "Freya/Asset/MeshPool.hpp"
#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Builders/RenderTargetBuilder.hpp"
#include "Freya/Core/BloomPass.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/CompositePass.hpp"
#include "Freya/Core/DebugDraw.hpp"
#include "Freya/Core/DebugDrawPass.hpp"
#include "Freya/Core/DeferredCompressedPass.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/IFrameStage.hpp"
#include "Freya/Core/IndirectDrawSystem.hpp"
#include "Freya/Core/Instance.hpp"
#include "Freya/Core/LightService.hpp"
#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/Core/PickPass.hpp"
#include "Freya/Core/RenderFrameContext.hpp"
#include "Freya/Core/RenderTarget.hpp"
#include "Freya/Core/ShadowPass.hpp"
#include "Freya/Core/SsaoPass.hpp"
#include "Freya/Core/SwapChain.hpp"
#include "Freya/Core/TaaPass.hpp"
#include "Freya/Core/TranslucentPass.hpp"
#include "Freya/Events/EventManager.hpp"

#include <limits>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace FREYA_NAMESPACE
{
    class Buffer;

    /**
     * @brief Stored draw command for reuse across rendering passes.
     */
    struct DrawCommand
    {
        std::uint32_t meshId;
        std::uint32_t materialId;
        std::uint32_t instanceCount;
        std::uint32_t firstInstance;
        std::uint32_t entityId    = kPickMissId;
        bool          castShadows = true;
    };

    class Renderer
    {
      public:
        Renderer(const skr::Arc<Instance>&               instance,
                 const skr::Arc<Surface>&                surface,
                 const skr::Arc<PhysicalDevice>&         physicalDevice,
                 const skr::Arc<Device>&                 device,
                 const skr::Arc<SwapChain>&              swapChain,
                 const skr::Arc<DeferredCompressedPass>& deferredPass,
                 const skr::Arc<BloomPass>&              bloomPass,
                 const skr::Arc<TaaPass>&                taaPass,
                 const skr::Arc<SsaoPass>&               ssaoPass,
                 const skr::Arc<CompositePass>&          compositePass,
                 const skr::Arc<DebugDrawPass>&          debugDrawPass,
                 const skr::Arc<CommandPool>&            commandPool,
                 const skr::Arc<LightService>&           lightService,
                 const skr::Arc<ShadowPass>&             shadowPass,
                 const skr::Arc<PickPass>&               pickPass,
                 const skr::Arc<skr::ServiceProvider>&   serviceProvider,
                 const skr::Arc<FreyaOptions>&           freyaOptions,
                 const skr::Arc<EventManager>&           eventManager);

        ~Renderer();

        void BeginFrame();

        /**
         * @brief Finish deferred scene rendering via registered frame stages.
         *
         * With an output target bound, begins a cleared swapchain UI render
         * pass and leaves the command buffer open for application drawing
         * (e.g. ImGui). Call Present() afterward. Without an output target,
         * the scene is already on the swapchain; Present() submits and
         * presents.
         */
        void EndScene();

        /**
         * @brief End any open UI pass, submit the command buffer, and present.
         *
         * Must be called once per BeginFrame (after EndScene, and after any
         * UI recording into GetCommandBuffer()).
         */
        void Present();

        /**
         * @brief EndScene() followed by Present().
         */
        void EndFrame();

        void RebuildSwapChain();

        void NextSubpass();
        void BindSubpass(std::uint32_t subpass);
        void AdvanceSubpass(std::uint32_t subpass);

        vk::PipelineLayout GetActivePipelineLayout() const;
        vk::RenderPass     GetActiveRenderPass() const;

        void SetShadowQuality(ShadowQuality quality);

        [[nodiscard]] ShadowQuality GetShadowQuality() const
        {
            return mShadowQuality;
        }

        void SetSsaoQuality(SsaoQuality quality);

        [[nodiscard]] SsaoQuality GetSsaoQuality() const
        {
            return mSsaoQuality;
        }

        void SetSsaoDebugView(SsaoDebugView view);

        [[nodiscard]] SsaoDebugView GetSsaoDebugView() const
        {
            return mFreyaOptions->ssaoDebugView;
        }

        void SetSsaoRadius(float radius);
        void SetSsaoBias(float bias);
        void SetSsaoPower(float power);
        void SetSsaoIntensity(float intensity);

        [[nodiscard]] float GetSsaoRadius() const
        {
            return mFreyaOptions->ssaoRadius;
        }
        [[nodiscard]] float GetSsaoBias() const
        {
            return mFreyaOptions->ssaoBias;
        }
        [[nodiscard]] float GetSsaoPower() const
        {
            return mFreyaOptions->ssaoPower;
        }
        [[nodiscard]] float GetSsaoIntensity() const
        {
            return mFreyaOptions->ssaoIntensity;
        }

        void SetTaaQuality(TaaQuality quality);

        [[nodiscard]] TaaQuality GetTaaQuality() const { return mTaaQuality; }

        void SetBloomQuality(BloomQuality quality);

        [[nodiscard]] BloomQuality GetBloomQuality() const
        {
            return mBloomQuality;
        }

        [[nodiscard]] bool GetVSync() const { return mFreyaOptions->vSync; }
        void               SetVSync(bool vSync);

        void                        SetSamples(std::uint32_t samples);
        [[nodiscard]] std::uint32_t GetSamples() const
        {
            return mFreyaOptions->sampleCount;
        }

        [[nodiscard]] float GetDrawDistance() const
        {
            return mFreyaOptions->drawDistance;
        }
        void SetDrawDistance(float drawDistance);

        void SetOutputTarget(const skr::Arc<RenderTarget>& target);
        void ClearOutputTarget();

        [[nodiscard]] skr::Arc<RenderTarget> GetOutputTarget() const
        {
            return mOutputTarget;
        }

        [[nodiscard]] vk::RenderPass GetUIRenderPass();

        [[nodiscard]] vk::CommandBuffer GetCommandBuffer();

        /**
         * @brief Upload the GPU-driven scene instance table for this frame.
         *
         * Prefer sorting by `(meshId, entityId)` so Freya forms one MDI
         * batch per mesh and resolves TAA `prevModel` by `entityId`. Cull
         * (approach A) frustum-tests each instance and atomic-compacts
         * survivors into dense batches for `drawIndexedIndirect`.
         */
        void UploadSceneInstances(std::span<const SceneInstanceUpload> uploads);

        void Draw(std::uint32_t meshId,
                  std::uint32_t materialId,
                  std::uint32_t entityId    = kPickMissId,
                  bool          castShadows = true);
        /**
         * @brief Queue draws into the scene upload list (compatibility).
         *
         * Expand into UploadSceneInstances internally on EndScene if
         * UploadSceneInstances was not called. Prefer UploadSceneInstances.
         */
        void DrawInstanced(std::uint32_t meshId,
                           std::uint32_t materialId,
                           size_t        instanceCount,
                           size_t        firstInstance = 0,
                           bool          castShadows   = true,
                           std::uint32_t entityId      = kPickMissId);

        /**
         * @brief Upload current-frame instance model matrices.
         *
         * Used with DrawInstanced. Prefer UploadSceneInstances for new code.
         */
        void SetInstanceModels(const glm::mat4* models, std::size_t count);

        /**
         * @brief Upload skinning bone palettes for the current in-flight
         * frame (current + previous slots for TAA).
         */
        void UploadBoneMatrices(std::span<const glm::mat4> bones);

        void ClearDrawCommands();
        void ExecuteDrawCommands(bool bindMaterials = true);
        void ExecutePickDrawCommands();
        void DispatchCull(const glm::mat4& viewProj, CullMode mode);

        void RequestPick(std::uint32_t x, std::uint32_t y);

        bool TryConsumePickResult(std::uint32_t& outEntityId);

        /**
         * @brief Insert a custom frame stage before the named stage.
         * @return false if beforeName was not found
         */
        bool InsertFrameStage(const char* beforeName, FrameStagePtr stage);

        /**
         * @brief Replace the stage with the given name.
         * @return false if name was not found
         */
        bool ReplaceFrameStage(const char* name, FrameStagePtr stage);

        [[nodiscard]] const std::vector<FrameStagePtr>& GetFrameStages() const
        {
            return mFrameStages;
        }

        glm::mat4 MakeProjection(float fovRadians, float aspect, float near,
                                 float far) const;

        void      ClearProjections();
        glm::mat4 CalculateProjectionMatrix(float near, float far) const;

        [[nodiscard]] const ProjectionUniformBuffer& GetCurrentProjection()
            const
        {
            return mCurrentProjection;
        }

        void UpdateProjection(ProjectionUniformBuffer& projectionUniformBuffer);

        void UpdateCamera(const glm::vec3& position,
                          const glm::vec3& target,
                          const glm::vec3& up);

        void SetAmbient(const glm::vec3& color, float intensity);

        void SetDebugDrawEnabled(bool enabled) { mDebugDrawEnabled = enabled; }
        [[nodiscard]] bool IsDebugDrawEnabled() const
        {
            return mDebugDrawEnabled;
        }
        [[nodiscard]] DebugDraw& GetDebugDraw() { return mDebugDraw; }

        void UpdateModel(const glm::mat4& model) const;

        [[nodiscard]] BufferBuilder       GetBufferBuilder() const;
        [[nodiscard]] RenderTargetBuilder GetRenderTargetBuilder() const;
        void BindBuffer(const skr::Arc<Buffer>& buffer) const;
        void BindMaterial(std::uint32_t materialId);

        std::uint32_t GetCurrentFrameIndex() const
        {
            return mSwapChain->GetCurrentFrameIndex();
        }
        std::uint32_t GetFrameCount() const
        {
            return mSwapChain->GetFrameCount();
        }

        skr::Arc<DeferredCompressedPass> GetDeferredPass() const
        {
            return mDeferredPass;
        }

      private:
        void blitBloomToFullRes(const skr::Arc<CommandPool>& commandPool,
                                std::uint32_t                frameIndex) const;

        [[nodiscard]] vk::Extent2D getRenderExtent() const;

        void rebuildSceneResources();

        void resizePickPass(vk::Extent2D extent);

        void beginComposite(std::uint32_t          frameIndex,
                            const skr::Arc<Image>& sceneImage,
                            bool                   tonemapHdr = false);

        ProjectionUniformBuffer prepareDeferredProjection(
            const ProjectionUniformBuffer& unjittered) const;

        void commitTaaHistory();

        void beginUIPass();

        void registerDefaultFrameStages();

        [[nodiscard]] RenderFrameContext makeFrameContext();

        skr::Arc<Image> createSsaoFallbackImage() const;

        void flushLegacyDrawCommands();

        skr::Arc<skr::ServiceProvider>   mServiceProvider;
        skr::Arc<Instance>               mInstance;
        skr::Arc<Surface>                mSurface;
        skr::Arc<PhysicalDevice>         mPhysicalDevice;
        skr::Arc<Device>                 mDevice;
        skr::Arc<SwapChain>              mSwapChain;
        skr::Arc<DeferredCompressedPass> mDeferredPass;
        skr::Arc<TranslucentPass>        mTranslucentPass;
        skr::Arc<BloomPass>              mBloomPass;
        skr::Arc<TaaPass>                mTaaPass;
        skr::Arc<SsaoPass>               mSsaoPass;
        skr::Arc<CompositePass>          mCompositePass;
        skr::Arc<DebugDrawPass>          mDebugDrawPass;
        DebugDraw                        mDebugDraw;
        bool                             mDebugDrawEnabled = false;
        skr::Arc<CommandPool>            mCommandPool;
        skr::Arc<LightService>           mLightService;
        skr::Arc<ShadowPass>             mShadowPass;
        skr::Arc<PickPass>               mPickPass;
        float                            mCameraNear = 1.0f;
        skr::Arc<EventManager>           mEventManager;
        skr::Arc<FreyaOptions>           mFreyaOptions;
        ShadowQuality                    mShadowQuality = ShadowQuality::High;
        SsaoQuality                      mSsaoQuality   = SsaoQuality::Medium;
        TaaQuality                       mTaaQuality    = TaaQuality::High;
        BloomQuality                     mBloomQuality  = BloomQuality::Medium;
        skr::Arc<RenderTarget>           mOutputTarget;
        bool                             mUIPassOpen = false;

        skr::Arc<MeshPool>           mMeshPool;
        skr::Arc<MaterialPool>       mMaterialPool;
        skr::Arc<IndirectDrawSystem> mIndirectDraw;

        vk::PipelineLayout mDrawPipelineLayoutOverride = {};

        std::optional<WindowResizeEvent> mResizeEvent;

        ProjectionUniformBuffer mCurrentProjection;
        glm::mat4               mPrevViewProjection { 1.0f };
        std::uint32_t           mTaaFrameIndex = 0;
        vk::Sampler             mBloomResultSampler;

        std::vector<skr::Arc<Image>> mBloomResultImages;
        skr::Arc<Image>              mSsaoFallbackImage;

        std::vector<DrawCommand>         mDrawCommands;
        std::vector<SceneInstanceUpload> mLegacyUploads;
        std::vector<glm::mat4>           mLegacyModels;
        bool                             mUsedUploadApi = false;

        std::vector<FrameStagePtr> mFrameStages;

        bool          mPickRequested        = false;
        std::uint32_t mPickX                = 0;
        std::uint32_t mPickY                = 0;
        bool          mPickAwaitingReadback = false;
    };

} // namespace FREYA_NAMESPACE
