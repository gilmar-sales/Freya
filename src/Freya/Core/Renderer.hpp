#pragma once

#include "Freya/Asset/MeshPool.hpp"
#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/Instance.hpp"
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
     * matrices, and binds rendering resources. Subscribes to WindowResizeEvent
     * for automatic swapchain rebuilding.
     *
     * @param instance        Vulkan instance
     * @param surface         Surface reference
     * @param physicalDevice  Physical device
     * @param device          Logical device
     * @param swapChain       Swapchain reference
     * @param renderPass      Render pass reference
     * @param commandPool     Command pool reference
     * @param serviceProvider Service provider for builder access
     * @param freyaOptions    Freya options reference
     * @param eventManager    Event manager reference
     */
    class Renderer
    {
      public:
        Renderer(const Ref<Instance>&             instance,
                 const Ref<Surface>&              surface,
                 const Ref<PhysicalDevice>&       physicalDevice,
                 const Ref<Device>&               device,
                 const Ref<SwapChain>&            swapChain,
                 const Ref<RenderPass>&           renderPass,
                 const Ref<CommandPool>&          commandPool,
                 const Ref<skr::ServiceProvider>& serviceProvider,
                 const Ref<FreyaOptions>&         freyaOptions,
                 const Ref<EventManager>&         eventManager) :
            mInstance(instance), mSurface(surface),
            mPhysicalDevice(physicalDevice), mDevice(device),
            mSwapChain(swapChain), mRenderPass(renderPass),
            mCommandPool(commandPool), mServiceProvider(serviceProvider),
            mFreyaOptions(freyaOptions), mEventManager(eventManager),
            mCurrentProjection({})
        {
            ClearProjections();

            mEventManager->Subscribe<WindowResizeEvent>(
                [this](WindowResizeEvent event) {
                    if (!event.handled)
                    {
                        mResizeEvent = event;
                    }
                });
        }

        ~Renderer();

        /**
         * @brief Begins a new frame: acquires image, resets command buffer,
         * begins render pass.
         *
         * Handles resize events by rebuilding swapchain before acquisition.
         * @throws std::runtime_error if swapchain acquisition fails
         */
        void BeginFrame();

        /**
         * @brief Ends the current frame: ends render pass, submits, presents.
         *
         * Rebuilds swapchain if presentation returns eErrorOutOfDateKHR or
         * eSuboptimalKHR.
         * @throws std::runtime_error if presentation fails
         */
        void EndFrame();

        /**
         * @brief Rebuilds the entire swapchain (waits for device idle).
         */
        void RebuildSwapChain();

        /**
         * @brief Returns current vertical synchronization state.
         */
        [[nodiscard]] bool GetVSync() const { return mFreyaOptions->vSync; }

        /**
         * @brief Sets vertical synchronization and rebuilds swapchain.
         * @param vSync true for vsync on, false for off
         */
        void SetVSync(bool vSync);

        /**
         * @brief Sets MSAA sample count, rebuilds render pass and swapchain.
         * @param samples Sample count (1, 2, 4, 8, 16, 32, 64)
         */
        void SetSamples(std::uint32_t samples);

        /**
         * @brief Returns current MSAA sample count.
         */
        [[nodiscard]] std::uint32_t GetSamples() const
        {
            return mFreyaOptions->sampleCount;
        }

        /**
         * @brief Returns the draw distance (far plane for frustum culling).
         */
        [[nodiscard]] float GetDrawDistance() const
        {
            return mFreyaOptions->drawDistance;
        }

        /**
         * @brief Sets the draw distance and clears cached projections.
         * @param drawDistance New draw distance in world units
         */
        void SetDrawDistance(float drawDistance);

        /**
         * @brief Clears all projection caches and recalculates from current
         * camera. Camera position is fixed at (0, 0, -10.1) looking forward.
         */
        void ClearProjections();

        /**
         * @brief Calculates a custom projection matrix with given near/far
         * planes.
         * @param near Near clipping plane
         * @param far  Far clipping plane
         * @return 4x4 perspective projection matrix
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
         * @param projectionUniformBuffer Projection data to upload
         */
        void UpdateProjection(ProjectionUniformBuffer& projectionUniformBuffer);

        /**
         * @brief Pushes model matrix as push constant to command buffer.
         * @param model 4x4 model transformation matrix
         */
        void UpdateModel(const glm::mat4& model) const;

        /**
         * @brief Returns a BufferBuilder configured with this renderer.
         */
        [[nodiscard]] BufferBuilder GetBufferBuilder() const;

        /**
         * @brief Binds a buffer to the current command buffer.
         * @param buffer Buffer to bind (vertex, index, or instance)
         */
        void BindBuffer(const Ref<Buffer>& buffer) const;

        /**
         * @brief Returns the current frame index.
         */
        std::uint32_t GetCurrentFrameIndex() const
        {
            return mSwapChain->GetCurrentFrameIndex();
        }

        /**
         * @brief Returns total frame count in swapchain.
         */
        std::uint32_t GetFrameCount() const
        {
            return mSwapChain->GetFrameCount();
        }

      private:
        Ref<skr::ServiceProvider> mServiceProvider;
        Ref<Instance>             mInstance;
        Ref<Surface>              mSurface;
        Ref<PhysicalDevice>       mPhysicalDevice;
        Ref<Device>               mDevice;
        Ref<SwapChain>            mSwapChain;
        Ref<RenderPass>           mRenderPass;
        Ref<CommandPool>          mCommandPool;
        Ref<EventManager>         mEventManager;
        Ref<FreyaOptions>         mFreyaOptions;

        std::optional<WindowResizeEvent> mResizeEvent;

        ProjectionUniformBuffer mCurrentProjection;
    };

} // namespace FREYA_NAMESPACE
