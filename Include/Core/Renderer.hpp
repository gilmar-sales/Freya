#pragma once

#include "Asset/MeshPool.hpp"
#include "CommandPool.hpp"
#include "Device.hpp"
#include "Factories/MeshPoolFactory.hpp"
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
        Renderer(std::shared_ptr<Instance> instance,
                 std::shared_ptr<Surface>
                     surface,
                 std::shared_ptr<PhysicalDevice>
                     physicalDevice,
                 std::shared_ptr<Device>
                     device,
                 std::shared_ptr<SwapChain>
                     swapChain,
                 std::shared_ptr<RenderPass>
                     renderPass,
                 std::shared_ptr<CommandPool>
                     commandPool,
                 std::vector<vk::Semaphore>
                     imageAvailableSemaphores,
                 std::vector<vk::Semaphore>
                     renderFinishedSemaphores,
                 std::vector<vk::Fence>
                                         inFlightFences,
                 bool                    vSync,
                 vk::SampleCountFlagBits samples,
                 vk::ClearColorValue clearColor, float drawDistance) :
            mInstance(instance),
            mSurface(surface), mPhysicalDevice(physicalDevice),
            mDevice(device), mSwapChain(swapChain), mRenderPass(renderPass),
            mCommandPool(commandPool),
            mImageAvailableSemaphores(imageAvailableSemaphores),
            mRenderFinishedSemaphores(renderFinishedSemaphores),
            mInFlightFences(inFlightFences), mVSync(vSync), mSamples(samples), mClearColor(clearColor), mDrawDistance(drawDistance), mCurrentFrameIndex(0)
        {
            ClearProjections();
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

        std::shared_ptr<MeshPoolFactory> GetMeshPoolFactory();
        BufferBuilder                    GetBufferBuilder();

        void BindBuffer(std::shared_ptr<Buffer> buffer);

      private:
        std::shared_ptr<Instance>       mInstance;
        std::shared_ptr<Surface>        mSurface;
        std::shared_ptr<PhysicalDevice> mPhysicalDevice;
        std::shared_ptr<Device>         mDevice;
        std::shared_ptr<SwapChain>      mSwapChain;
        std::shared_ptr<RenderPass>     mRenderPass;
        std::shared_ptr<CommandPool>    mCommandPool;

        std::vector<vk::Semaphore> mImageAvailableSemaphores;
        std::vector<vk::Semaphore> mRenderFinishedSemaphores;
        std::vector<vk::Fence>     mInFlightFences;

        vk::SampleCountFlagBits mSamples;
        vk::ClearColorValue     mClearColor;
        float                   mDrawDistance;

        vk::ResultValue<std::uint32_t> mImageIndexResult = { vk::Result::eSuccess, 0 };
        bool                           mVSync;

        std::shared_ptr<Buffer> mVertexBuffer;
        std::shared_ptr<Buffer> mIndexBuffer;
        std::uint32_t           mCurrentFrameIndex;

        ProjectionUniformBuffer mCurrentProjection;
    };

} // namespace FREYA_NAMESPACE
