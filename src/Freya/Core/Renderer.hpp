#pragma once

#include "Freya/Asset/MaterialPool.hpp"
#include "Freya/Asset/MeshPool.hpp"
#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Builders/RenderTargetBuilder.hpp"
#include "Freya/Core/BloomPass.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/CompositePass.hpp"
#include "Freya/Core/DeferredCompressedPass.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/Instance.hpp"
#include "Freya/Core/LightService.hpp"
#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/Core/RenderPass.hpp"
#include "Freya/Core/RenderTarget.hpp"
#include "Freya/Core/SwapChain.hpp"
#include "Freya/Events/EventManager.hpp"

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
    };

    class Renderer
    {
      public:
        Renderer(const skr::Arc<Instance>&               instance,
                 const skr::Arc<Surface>&                surface,
                 const skr::Arc<PhysicalDevice>&         physicalDevice,
                 const skr::Arc<Device>&                 device,
                 const skr::Arc<SwapChain>&              swapChain,
                 const skr::Arc<RenderPass>&             forwardPass,
                 const skr::Arc<DeferredCompressedPass>& deferredPass,
                 const skr::Arc<BloomPass>&              bloomPass,
                 const skr::Arc<CompositePass>&          compositePass,
                 const skr::Arc<CommandPool>&            commandPool,
                 const skr::Arc<LightService>&           lightService,
                 const skr::Arc<skr::ServiceProvider>&   serviceProvider,
                 const skr::Arc<FreyaOptions>&           freyaOptions,
                 const skr::Arc<EventManager>&           eventManager,
                 const skr::Arc<Image>& forwardColorImage   = nullptr,
                 const skr::Arc<Image>& forwardResolveImage = nullptr);

        ~Renderer();

        void BeginFrame();

        /**
         * @brief Finish scene rendering (deferred/forward, bloom, composite).
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
         *
         * Convenience for apps that do not record UI between scene and
         * present. With an output target, the UI pass is begun and ended
         * empty (cleared swapchain).
         */
        void EndFrame();

        void RebuildSwapChain();

        void NextSubpass();
        void BindSubpass(std::uint32_t subpass);
        void AdvanceSubpass(std::uint32_t subpass);

        vk::PipelineLayout GetActivePipelineLayout() const;
        vk::RenderPass     GetActiveRenderPass() const;

        [[nodiscard]] bool IsDeferred() const
        {
            return mFreyaOptions->renderingStrategy ==
                   RenderingStrategy::Deferred;
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

        /**
         * @brief Redirect scene composite output to an offscreen target.
         *
         * Scene/bloom size to the target extent. After EndScene the swapchain
         * UI pass is open for the app to draw (e.g. ImGui); call Present to
         * submit.
         */
        void SetOutputTarget(const skr::Arc<RenderTarget>& target);

        /**
         * @brief Restore default composite destination (swapchain).
         */
        void ClearOutputTarget();

        [[nodiscard]] skr::Arc<RenderTarget> GetOutputTarget() const
        {
            return mOutputTarget;
        }

        /**
         * @brief Swapchain render pass used for UI after EndScene with an
         * output target (Clear → ColorAttachment → PresentSrc).
         *
         * Use this when initializing ImGui (or another UI backend) so its
         * pipeline matches the open pass between EndScene and Present.
         */
        [[nodiscard]] vk::RenderPass GetUIRenderPass();

        /**
         * @brief Current frame command buffer (valid between BeginFrame and
         * Present).
         */
        [[nodiscard]] vk::CommandBuffer GetCommandBuffer();

        // Draw commands with material binding
        void Draw(std::uint32_t meshId, std::uint32_t materialId);
        void DrawInstanced(std::uint32_t meshId,
                           std::uint32_t materialId,
                           size_t        instanceCount,
                           size_t        firstInstance = 0);

        // Stored draw command management
        void ClearDrawCommands();
        void ExecuteDrawCommands(bool bindMaterials = true);

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

        /**
         * @brief Sets flat ambient fill stored in ProjectionUniformBuffer.
         */
        void SetAmbient(const glm::vec3& color, float intensity);

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

        skr::Arc<RenderPass> GetForwardPass() const { return mForwardPass; }

        skr::Arc<DeferredCompressedPass> GetDeferredPass() const
        {
            return mDeferredPass;
        }

      private:
        void blitBloomToFullRes(const skr::Arc<CommandPool>& commandPool) const;

        /**
         * @brief Creates forward offscreen render pass and resources.
         * Called during construction and swapchain rebuild.
         */
        void createForwardOffscreenResources();

        /**
         * @brief Destroys forward offscreen resources.
         */
        void destroyForwardOffscreenResources();

        /**
         * @brief Extent used for scene/bloom/composite sizing.
         */
        [[nodiscard]] vk::Extent2D getRenderExtent() const;

        /**
         * @brief Rebuild bloom/deferred/forward scene resources at render
         * extent (keeps current swapchain).
         */
        void rebuildSceneResources();

        void beginComposite(std::uint32_t          frameIndex,
                            const skr::Arc<Image>& opaqueImage,
                            const skr::Arc<Image>& translucentImage);

        /**
         * @brief Begin a cleared swapchain render pass for application UI.
         * Leaves the pass open until Present().
         */
        void beginUIPass();

        skr::Arc<skr::ServiceProvider>   mServiceProvider;
        skr::Arc<Instance>               mInstance;
        skr::Arc<Surface>                mSurface;
        skr::Arc<PhysicalDevice>         mPhysicalDevice;
        skr::Arc<Device>                 mDevice;
        skr::Arc<SwapChain>              mSwapChain;
        skr::Arc<RenderPass>             mForwardPass;
        skr::Arc<DeferredCompressedPass> mDeferredPass;
        skr::Arc<BloomPass>              mBloomPass;
        skr::Arc<CompositePass>          mCompositePass;
        skr::Arc<CommandPool>            mCommandPool;
        skr::Arc<LightService>           mLightService;
        skr::Arc<EventManager>           mEventManager;
        skr::Arc<FreyaOptions>           mFreyaOptions;
        skr::Arc<RenderTarget>           mOutputTarget;
        bool                             mUIPassOpen = false;

        // Mesh and Material pools for draw commands
        skr::Arc<MeshPool>     mMeshPool;
        skr::Arc<MaterialPool> mMaterialPool;

        std::optional<WindowResizeEvent> mResizeEvent;

        ProjectionUniformBuffer mCurrentProjection;
        vk::Sampler             mBloomResultSampler;

        // Full-res bloom result image (blit target from half-res bloom up)
        skr::Arc<Image> mBloomResultImage;

        // Forward offscreen resources (for bloom+composite in forward mode)
        skr::Arc<Image>              mForwardColorImage;
        skr::Arc<Image>              mForwardResolveImage;
        skr::Arc<Image>              mForwardDepthImage;
        vk::RenderPass               mForwardOffscreenRenderPass;
        std::vector<vk::Framebuffer> mForwardOffscreenFramebuffers;

        // Stored draw commands for reuse across passes (depth pre-pass,
        // gbuffer)
        std::vector<DrawCommand> mDrawCommands;
    };

} // namespace FREYA_NAMESPACE
