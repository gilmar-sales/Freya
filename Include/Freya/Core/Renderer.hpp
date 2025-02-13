#pragma once

#include "Freya/Asset/MeshPool.hpp"
#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Core/CommandPool.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/ForwardPass.hpp"
#include "Freya/Core/Instance.hpp"
#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/Core/Surface.hpp"
#include "Freya/Core/SwapChain.hpp"
#include "Freya/Events/EventManager.hpp"
#include "Freya/Factories/MeshPoolFactory.hpp"
#include "Freya/Factories/TexturePoolFactory.hpp"

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    class Buffer;

    class Renderer
    {
      public:
        Renderer(const Ref<Instance>&              instance,
                 const Ref<Surface>&               surface,
                 const Ref<PhysicalDevice>&        physicalDevice,
                 const Ref<Device>&                device,
                 const Ref<SwapChain>&             swapChain,
                 const Ref<ForwardPass>&           renderPass,
                 const Ref<CommandPool>&           commandPool,
                 const std::vector<vk::Semaphore>& imageAvailableSemaphores,
                 const std::vector<vk::Semaphore>& renderFinishedSemaphores,
                 const std::vector<vk::Fence>&     inFlightFences,
                 const bool                        vSync,
                 const vk::SampleCountFlagBits     samples,
                 const vk::ClearColorValue         clearColor,
                 const float                       drawDistance,
                 const Ref<EventManager>&          eventManager) :
            mInstance(instance), mSurface(surface),
            mPhysicalDevice(physicalDevice), mDevice(device),
            mSwapChain(swapChain), mRenderPass(renderPass),
            mCommandPool(commandPool), mEventManager(eventManager),
            mImageAvailableSemaphores(imageAvailableSemaphores),
            mRenderFinishedSemaphores(renderFinishedSemaphores),
            mInFlightFences(inFlightFences), mSamples(samples),
            mClearColor(clearColor), mDrawDistance(drawDistance), mVSync(vSync),
            mCurrentFrameIndex(0), mCurrentProjection({})
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

        void BeginFrame();

        void EndFrame();

        void               RebuildSwapChain();
        [[nodiscard]] bool GetVSync() const { return mVSync; }
        void               SetVSync(bool vSync);
        void               SetSamples(vk::SampleCountFlagBits samples);
        [[nodiscard]] vk::SampleCountFlagBits GetSamples() const
        {
            return mSamples;
        }
        [[nodiscard]] float GetDrawDistance() const { return mDrawDistance; }

        void      SetDrawDistance(float drawDistance);
        void      ClearProjections();
        glm::mat4 CalculateProjectionMatrix(float near, float far) const;

        [[nodiscard]] const ProjectionUniformBuffer& GetCurrentProjection()
            const
        {
            return mCurrentProjection;
        }
        void UpdateProjection(ProjectionUniformBuffer& projectionUniformBuffer);
        void UpdateModel(const glm::mat4& model) const;

        Ref<MeshPoolFactory>        GetMeshPoolFactory();
        Ref<TexturePoolFactory>     GetTexturePoolFactory();
        [[nodiscard]] BufferBuilder GetBufferBuilder() const;

        void BindBuffer(const Ref<Buffer>& buffer) const;

      private:
        Ref<Instance>       mInstance;
        Ref<Surface>        mSurface;
        Ref<PhysicalDevice> mPhysicalDevice;
        Ref<Device>         mDevice;
        Ref<SwapChain>      mSwapChain;
        Ref<ForwardPass>    mRenderPass;
        Ref<CommandPool>    mCommandPool;
        Ref<EventManager>   mEventManager;

        std::vector<vk::Semaphore> mImageAvailableSemaphores;
        std::vector<vk::Semaphore> mRenderFinishedSemaphores;
        std::vector<vk::Fence>     mInFlightFences;

        vk::SampleCountFlagBits mSamples;
        vk::ClearColorValue     mClearColor;
        float                   mDrawDistance;

        vk::ResultValue<std::uint32_t> mImageIndexResult = {
            vk::Result::eSuccess, 0
        };
        bool mVSync;

        Ref<Buffer> mVertexBuffer;
        Ref<Buffer> mIndexBuffer;

        std::optional<WindowResizeEvent> mResizeEvent;
        std::uint32_t                    mCurrentFrameIndex;

        ProjectionUniformBuffer mCurrentProjection;
    };

} // namespace FREYA_NAMESPACE
