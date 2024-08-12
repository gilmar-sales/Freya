#pragma once

#include "Asset/MeshPool.hpp"
#include "CommandPool.hpp"
#include "Device.hpp"
#include "Events/EventManager.hpp"
#include "Factories/MeshPoolFactory.hpp"
#include "Factories/TexturePoolFactory.hpp"
#include "Instance.hpp"
#include "PhysicalDevice.hpp"
#include "RenderPass.hpp"
#include "Surface.hpp"
#include "SwapChain.hpp"

#include <Builders/BufferBuilder.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    class Buffer;

    class Renderer
    {
      public:
        Renderer(Ref<Instance> instance,
                 Ref<Surface>
                     surface,
                 Ref<PhysicalDevice>
                     physicalDevice,
                 Ref<Device>
                     device,
                 Ref<SwapChain>
                     swapChain,
                 Ref<RenderPass>
                     renderPass,
                 Ref<CommandPool>
                     commandPool,
                 std::vector<vk::Semaphore>
                     imageAvailableSemaphores,
                 std::vector<vk::Semaphore>
                     renderFinishedSemaphores,
                 std::vector<vk::Fence>
                                         inFlightFences,
                 bool                    vSync,
                 vk::SampleCountFlagBits samples,
                 vk::ClearColorValue clearColor, float drawDistance,
                 Ref<EventManager> eventManager) :
            mInstance(instance),
            mSurface(surface), mPhysicalDevice(physicalDevice),
            mDevice(device), mSwapChain(swapChain), mRenderPass(renderPass),
            mCommandPool(commandPool),
            mImageAvailableSemaphores(imageAvailableSemaphores),
            mRenderFinishedSemaphores(renderFinishedSemaphores),
            mInFlightFences(inFlightFences), mVSync(vSync), mSamples(samples), mClearColor(clearColor), mDrawDistance(drawDistance), mCurrentFrameIndex(0), mEventManager(eventManager)
        {
            ClearProjections();

            mEventManager->Subscribe<WindowResizeEvent>([this](WindowResizeEvent event) {
                if (!event.handled)
                {
                    mResizeEvent = event;
                }
            });
        }

        ~Renderer();

        void BeginFrame();

        void EndFrame();

        void                    RebuildSwapChain();
        bool                    GetVSync() { return mVSync; }
        void                    SetVSync(bool vSync);
        void                    SetSamples(vk::SampleCountFlagBits samples);
        vk::SampleCountFlagBits GetSamples() { return mSamples; }
        float                   GetDrawDistance() { return mDrawDistance; }

        void SetDrawDistance(float drawDistance);
        void ClearProjections();

        const ProjectionUniformBuffer& GetCurrentProjection() { return mCurrentProjection; }
        void                           UpdateProjection(ProjectionUniformBuffer& projectionUniformBuffer);
        void                           UpdateModel(glm::mat4& model);

        Ref<MeshPoolFactory>    GetMeshPoolFactory();
        Ref<TexturePoolFactory> GetTexturePoolFactory();
        BufferBuilder           GetBufferBuilder();

        void BindBuffer(Ref<Buffer> buffer);

      private:
        Ref<Instance>       mInstance;
        Ref<Surface>        mSurface;
        Ref<PhysicalDevice> mPhysicalDevice;
        Ref<Device>         mDevice;
        Ref<SwapChain>      mSwapChain;
        Ref<RenderPass>     mRenderPass;
        Ref<CommandPool>    mCommandPool;
        Ref<EventManager>   mEventManager;

        std::vector<vk::Semaphore> mImageAvailableSemaphores;
        std::vector<vk::Semaphore> mRenderFinishedSemaphores;
        std::vector<vk::Fence>     mInFlightFences;

        vk::SampleCountFlagBits mSamples;
        vk::ClearColorValue     mClearColor;
        float                   mDrawDistance;

        vk::ResultValue<std::uint32_t> mImageIndexResult = { vk::Result::eSuccess, 0 };
        bool                           mVSync;

        Ref<Buffer> mVertexBuffer;
        Ref<Buffer> mIndexBuffer;

        std::optional<WindowResizeEvent> mResizeEvent;
        std::uint32_t                    mCurrentFrameIndex;

        ProjectionUniformBuffer mCurrentProjection;
    };

} // namespace FREYA_NAMESPACE
