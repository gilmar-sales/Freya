#pragma once

#include "Freya/Asset/MaterialPool.hpp"
#include "Freya/Asset/MeshPool.hpp"
#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Core/BloomPass.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/CompositePass.hpp"
#include "Freya/Core/DeferredCompressedPass.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/Instance.hpp"
#include "Freya/Core/LightService.hpp"
#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/Core/RenderPass.hpp"
#include "Freya/Core/Surface.hpp"
#include "Freya/Core/SwapChain.hpp"
#include "Freya/Events/EventManager.hpp"

namespace FREYA_NAMESPACE
{
    class Buffer;

    class Renderer
    {
      public:
        Renderer(const Ref<Instance>&               instance,
                 const Ref<Surface>&                surface,
                 const Ref<PhysicalDevice>&         physicalDevice,
                 const Ref<Device>&                 device,
                 const Ref<SwapChain>&              swapChain,
                 const Ref<RenderPass>&             forwardPass,
                 const Ref<DeferredCompressedPass>& deferredPass,
                 const Ref<BloomPass>&              bloomPass,
                 const Ref<CompositePass>&          compositePass,
                 const Ref<CommandPool>&            commandPool,
                 const Ref<LightService>&           lightService,
                 const Ref<skr::ServiceProvider>&   serviceProvider,
                 const Ref<FreyaOptions>&           freyaOptions,
                 const Ref<EventManager>&           eventManager,
                 const Ref<Image>&                  forwardColorImage = nullptr,
                 const Ref<Image>& forwardResolveImage = nullptr);

        ~Renderer();

        void BeginFrame();
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

        // Draw commands with material binding
        void Draw(std::uint32_t meshId, std::uint32_t materialId);
        void DrawInstanced(std::uint32_t meshId,
                           std::uint32_t materialId,
                           size_t        instanceCount,
                           size_t        firstInstance = 0);

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

        void UpdateModel(const glm::mat4& model) const;

        [[nodiscard]] BufferBuilder GetBufferBuilder() const;
        void                        BindBuffer(const Ref<Buffer>& buffer) const;
        void                        BindMaterial(std::uint32_t materialId);

        std::uint32_t GetCurrentFrameIndex() const
        {
            return mSwapChain->GetCurrentFrameIndex();
        }
        std::uint32_t GetFrameCount() const
        {
            return mSwapChain->GetFrameCount();
        }

        Ref<RenderPass> GetForwardPass() const { return mForwardPass; }

        Ref<DeferredCompressedPass> GetDeferredPass() const
        {
            return mDeferredPass;
        }

      private:
        void blitBloomToFullRes(const Ref<CommandPool>& commandPool) const;

        /**
         * @brief Creates forward offscreen render pass and resources.
         * Called during construction and swapchain rebuild.
         */
        void createForwardOffscreenResources();

        /**
         * @brief Destroys forward offscreen resources.
         */
        void destroyForwardOffscreenResources();

        Ref<skr::ServiceProvider>   mServiceProvider;
        Ref<Instance>               mInstance;
        Ref<Surface>                mSurface;
        Ref<PhysicalDevice>         mPhysicalDevice;
        Ref<Device>                 mDevice;
        Ref<SwapChain>              mSwapChain;
        Ref<RenderPass>             mForwardPass;
        Ref<DeferredCompressedPass> mDeferredPass;
        Ref<BloomPass>              mBloomPass;
        Ref<CompositePass>          mCompositePass;
        Ref<CommandPool>            mCommandPool;
        Ref<LightService>           mLightService;
        Ref<EventManager>           mEventManager;
        Ref<FreyaOptions>           mFreyaOptions;

        // Mesh and Material pools for draw commands
        Ref<MeshPool>     mMeshPool;
        Ref<MaterialPool> mMaterialPool;

        std::optional<WindowResizeEvent> mResizeEvent;

        ProjectionUniformBuffer mCurrentProjection;
        vk::Sampler             mBloomResultSampler;

        // Full-res bloom result image (blit target from half-res bloom up)
        Ref<Image> mBloomResultImage;

        // Forward offscreen resources (for bloom+composite in forward mode)
        Ref<Image>                   mForwardColorImage;
        Ref<Image>                   mForwardResolveImage;
        Ref<Image>                   mForwardDepthImage;
        vk::RenderPass               mForwardOffscreenRenderPass;
        std::vector<vk::Framebuffer> mForwardOffscreenFramebuffers;
    };

} // namespace FREYA_NAMESPACE
