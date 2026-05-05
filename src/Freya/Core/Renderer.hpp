#pragma once

#include "Freya/Asset/MeshPool.hpp"
#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Core/CommandPool.hpp"
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

    /**
     * @brief Main rendering orchestrator managing frame lifecycle and state.
     *
     * Coordinates BeginFrame/EndFrame, manages swapchain rebuilds, projection
     * matrices, and binds rendering resources. Supports both Forward and
     * Deferred rendering strategies.
     */
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
                 const Ref<CommandPool>&            commandPool,
                 const Ref<LightService>&           lightService,
                 const Ref<skr::ServiceProvider>&   serviceProvider,
                 const Ref<FreyaOptions>&           freyaOptions,
                 const Ref<EventManager>&           eventManager);

        ~Renderer();

        /**
         * @brief Begins a new frame: acquires swapchain image, resets command
         * buffer, begins the active render pass.
         */
        void BeginFrame();

        /**
         * @brief Ends the current frame: ends render pass, submits, presents.
         */
        void EndFrame();

        /**
         * @brief Rebuilds the entire swapchain and recreated deferred
         * framebuffers (if needed).
         */
        void RebuildSwapChain();

        /**
         * @brief Advances to the next subpass in a deferred render pass.
         * No-op in forward mode.
         */
        void NextSubpass();

        /**
         * @brief Binds the appropriate pipeline for the given subpass.
         * No-op in forward mode.
         * @param subpass Subpass index (e.g., DeferredGBufferPass)
         */
        void BindSubpass(std::uint32_t subpass);

        /**
         * @brief Convenience: NextSubpass + BindSubpass.
         * @param subpass Target subpass index
         */
        void AdvanceSubpass(std::uint32_t subpass);

        /**
         * @brief Returns the currently active pipeline layout for push
         * constants / descriptor binding.
         */
        vk::PipelineLayout GetActivePipelineLayout() const;

        /**
         * @brief Returns the currently active render pass (for forward) or
         * the deferred render pass handle.
         */
        vk::RenderPass GetActiveRenderPass() const;

        /**
         * @brief Returns true if currently in deferred mode.
         */
        [[nodiscard]] bool IsDeferred() const
        {
            return mFreyaOptions->renderingStrategy ==
                   RenderingStrategy::Deferred;
        }

        /** @name VSync control */
        //@{
        [[nodiscard]] bool GetVSync() const { return mFreyaOptions->vSync; }
        void               SetVSync(bool vSync);
        //@}

        /** @name MSAA control */
        //@{
        void                        SetSamples(std::uint32_t samples);
        [[nodiscard]] std::uint32_t GetSamples() const
        {
            return mFreyaOptions->sampleCount;
        }
        //@}

        /** @name Draw distance */
        //@{
        [[nodiscard]] float GetDrawDistance() const
        {
            return mFreyaOptions->drawDistance;
        }
        void SetDrawDistance(float drawDistance);
        //@}

        /**
         * @brief Builds a reverse-Z perspective projection matrix.
         */
        glm::mat4 MakeProjection(float fovRadians, float aspect, float near,
                                 float far) const;

        /**
         * @brief Clears cached projection and recalculates from current
         * camera settings.
         */
        void ClearProjections();

        /**
         * @brief Calculates a custom reverse-Z projection matrix.
         */
        glm::mat4 CalculateProjectionMatrix(float near, float far) const;

        /**
         * @brief Returns the current projection uniform buffer.
         */
        [[nodiscard]] const ProjectionUniformBuffer& GetCurrentProjection()
            const
        {
            return mCurrentProjection;
        }

        /**
         * @brief Updates projection uniform buffer for the current frame.
         */
        void UpdateProjection(ProjectionUniformBuffer& projectionUniformBuffer);

        /**
         * @brief Updates the camera view matrix.
         */
        void UpdateCamera(const glm::vec3& position,
                          const glm::vec3& target,
                          const glm::vec3& up);

        /**
         * @brief Pushes model matrix as push constant to the active pipeline.
         */
        void UpdateModel(const glm::mat4& model) const;

        /**
         * @brief Returns a BufferBuilder configured with this renderer.
         */
        [[nodiscard]] BufferBuilder GetBufferBuilder() const;

        /**
         * @brief Binds a buffer to the current command buffer.
         */
        void BindBuffer(const Ref<Buffer>& buffer) const;

        /** @name Frame index queries */
        //@{
        std::uint32_t GetCurrentFrameIndex() const
        {
            return mSwapChain->GetCurrentFrameIndex();
        }
        std::uint32_t GetFrameCount() const
        {
            return mSwapChain->GetFrameCount();
        }
        //@}

        /**
         * @brief Returns the forward render pass (for outside use).
         */
        Ref<RenderPass> GetForwardPass() const { return mForwardPass; }

        /**
         * @brief Returns the deferred pass (may be null in forward mode).
         */
        Ref<DeferredCompressedPass> GetDeferredPass() const
        {
            return mDeferredPass;
        }

      private:
        Ref<skr::ServiceProvider>   mServiceProvider;
        Ref<Instance>               mInstance;
        Ref<Surface>                mSurface;
        Ref<PhysicalDevice>         mPhysicalDevice;
        Ref<Device>                 mDevice;
        Ref<SwapChain>              mSwapChain;
        Ref<RenderPass>             mForwardPass;
        Ref<DeferredCompressedPass> mDeferredPass;
        Ref<CommandPool>            mCommandPool;
        Ref<LightService>           mLightService;
        Ref<EventManager>           mEventManager;
        Ref<FreyaOptions>           mFreyaOptions;

        std::optional<WindowResizeEvent> mResizeEvent;

        ProjectionUniformBuffer mCurrentProjection;
    };

} // namespace FREYA_NAMESPACE
