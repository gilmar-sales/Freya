#pragma once

#include "Freya/Core/Renderer.hpp"

#include "Freya/Asset/BoneMatrixResources.hpp"
#include "Freya/Asset/MeshPool.hpp"
#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Builders/RenderTargetBuilder.hpp"
#include "Freya/Core/BillboardPass.hpp"
#include "Freya/Core/BloomPass.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/CompositePass.hpp"
#include "Freya/Core/DebugDrawPass.hpp"
#include "Freya/Core/DeferredCompressedPass.hpp"
#include "Freya/Core/Device.hpp"
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

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace FREYA_NAMESPACE
{
    class Buffer;

    struct DrawCommand
    {
        std::uint32_t meshId;
        std::uint32_t materialId;
        std::uint32_t instanceCount;
        std::uint32_t firstInstance;
        std::uint32_t entityId    = kPickMissId;
        bool          castShadows = true;
    };

    class Renderer::Impl
    {
      public:
        Impl(const skr::Arc<Instance>&               instance,
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
             const skr::Arc<GpuAnimPass>&            gpuAnimPass,
             const skr::Arc<CommandPool>&            commandPool,
             const skr::Arc<LightService>&           lightService,
             const skr::Arc<ShadowPass>&             shadowPass,
             const skr::Arc<PickPass>&               pickPass,
             const skr::Arc<skr::ServiceProvider>&   serviceProvider,
             const skr::Arc<FreyaOptions>&           freyaOptions,
             const skr::Arc<EventManager>&           eventManager);

        ~Impl();

        void BeginFrame();
        void EndScene();
        void Present();
        void EndFrame();
        void RebuildSwapChain();

        void NextSubpass();
        void BindSubpass(std::uint32_t subpass);
        void AdvanceSubpass(std::uint32_t subpass);

        vk::PipelineLayout GetActivePipelineLayout() const;
        vk::RenderPass     GetActiveRenderPass() const;

        void SetShadowQuality(ShadowQuality quality);
        void SetSsaoQuality(SsaoQuality quality);
        void SetSsaoDebugView(SsaoDebugView view);
        void SetSsaoRadius(float radius);
        void SetSsaoBias(float bias);
        void SetSsaoPower(float power);
        void SetSsaoIntensity(float intensity);
        void SetTaaQuality(TaaQuality quality);
        void SetBloomQuality(BloomQuality quality);
        void SetVSync(bool vSync);
        void SetSamples(std::uint32_t samples);
        void SetDrawDistance(float drawDistance);

        void SetOutputTarget(const skr::Arc<RenderTarget>& target);
        void ClearOutputTarget();

        [[nodiscard]] vk::RenderPass    GetUIRenderPass();
        [[nodiscard]] vk::CommandBuffer GetCommandBuffer();

        void UploadSceneInstances(std::span<const SceneInstanceUpload> uploads);

        void Draw(std::uint32_t meshId,
                  std::uint32_t materialId,
                  std::uint32_t entityId    = kPickMissId,
                  bool          castShadows = true);

        void DrawInstanced(std::uint32_t meshId,
                           std::uint32_t materialId,
                           size_t        instanceCount,
                           size_t        firstInstance = 0,
                           bool          castShadows   = true,
                           std::uint32_t entityId      = kPickMissId);

        void SetInstanceModels(const glm::mat4* models, std::size_t count);
        void UploadBoneMatrices(std::span<const glm::mat4> bones);

        void ClearDrawCommands();
        void ExecuteDrawCommands(bool bindMaterials = true);
        void ExecutePickDrawCommands();
        void DispatchCull(const glm::mat4& viewProj, CullMode mode);

        void RequestPick(std::uint32_t x, std::uint32_t y);
        bool TryConsumePickResult(std::uint32_t& outEntityId);

        bool InsertFrameStage(const char* beforeName, FrameStagePtr stage);
        bool ReplaceFrameStage(const char* name, FrameStagePtr stage);

        glm::mat4 MakeProjection(float fovRadians, float aspect, float near,
                                 float far) const;

        void      ClearProjections();
        glm::mat4 CalculateProjectionMatrix(float near, float far) const;

        void UpdateProjection(ProjectionUniformBuffer& projectionUniformBuffer);

        void UpdateCamera(const glm::vec3& position,
                          const glm::vec3& target,
                          const glm::vec3& up);

        void SetAmbient(const glm::vec3& color, float intensity);

        void RebuildGpuAnimPass();

        bool ReadbackGpuAnimBones(std::uint32_t frameIndex,
                                  std::uint32_t boneOffset,
                                  std::span<glm::mat4>
                                      out);

        bool DispatchGpuAnimImmediate(
            std::span<const GpuAnimInstance> instances,
            std::uint32_t                    frameIndex);

        void SetGpuAnimJointExtract(
            std::span<const GpuJointExtractRequest> requests);

        bool PollGpuAnimJointExtract(std::span<GpuJointExtractSample> out,
                                     std::uint32_t* outCount = nullptr);

        bool PollGpuAnimTiming(GpuAnimTimingSample& out);

        void SetGpuAnimCopyPrevBones(bool enabled);
        void UploadGpuAnimInstances(std::span<const GpuAnimInstance> instances);
        void CaptureGpuAnimDebugSnapshot(GpuAnimDebugSnapshot& out) const;
        [[nodiscard]] std::uint32_t FindGpuAnimClipSlot(
            std::uint64_t key) const;
        [[nodiscard]] std::uint32_t EnsureGpuAnimClipResident(
            std::uint64_t key, const BakedClip& clip);
        [[nodiscard]] std::uint32_t GetGpuAnimResidentClipCount() const;
        [[nodiscard]] std::uint32_t GetGpuAnimJointsPerClipSlot() const;

        void UploadGpuAnimSkeleton(const GpuSkeletonPack& skeleton);
        void ResetGpuAnimClipCache();
        bool UploadGpuAnimClipSlot(std::uint32_t slot, std::uint64_t key,
                                   const BakedClip& clip);
        void PinGpuAnimClipSlot(std::uint32_t slot, bool pinned);
        void UploadGpuAnimBoneMask(std::span<const float> weights);
        void UploadGpuAnimRestJoints(std::span<const GpuFloatJoint> joints);
        void UploadGpuAnimRestJoints(std::span<const GpuQuantJoint> joints);
        void SetGpuAnimRigIndices(std::uint32_t lookJoint, std::uint32_t ikRoot,
                                  std::uint32_t ikMid, std::uint32_t ikTip,
                                  std::uint32_t rootJoint,
                                  glm::vec3     lookLocalForward,
                                  float lookMaxYawRad, float lookMaxPitchRad);

        void UpdateModel(const glm::mat4& model) const;

        [[nodiscard]] BufferBuilder       GetBufferBuilder() const;
        [[nodiscard]] RenderTargetBuilder GetRenderTargetBuilder() const;
        void BindBuffer(const skr::Arc<Buffer>& buffer) const;

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
        skr::Arc<BillboardPass>          mBillboardPass;
        skr::Arc<GpuAnimPass>            mGpuAnimPass;
        DebugDraw                        mDebugDraw;
        BillboardDraw                    mBillboardDraw;
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
