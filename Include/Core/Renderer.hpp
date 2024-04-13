#pragma once

#include "Asset/MeshPool.hpp"
#include "CommandPool.hpp"
#include "Device.hpp"
#include "Instance.hpp"
#include "PhysicalDevice.hpp"
#include "RenderPass.hpp"
#include "Surface.hpp"
#include "SwapChain.hpp"

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
                 vk::SampleCountFlagBits samples) :
            mInstance(instance),
            mSurface(surface), mPhysicalDevice(physicalDevice),
            mDevice(device), mSwapChain(swapChain), mRenderPass(renderPass),
            mCommandPool(commandPool),
            mImageAvailableSemaphores(imageAvailableSemaphores),
            mRenderFinishedSemaphores(renderFinishedSemaphores),
            mInFlightFences(inFlightFences), mVSync(vSync), mSamples(samples), mCurrentFrameIndex(0)
        {
            mMeshPool =
                std::make_shared<MeshPool>(mDevice, mPhysicalDevice, mCommandPool);

            InitMeshes();
        }

        ~Renderer();

        void InitMeshes();

        void BeginFrame();

        void Draw();

        void EndFrame();

        void RebuildSwapChain();
        void SetVSync(bool vSync);
        void SetSamples(vk::SampleCountFlagBits samples);

        void UpdateProjection(ProjectionUniformBuffer& projectionUniformBuffer);

        std::shared_ptr<MeshPool> GetMeshPool() { return mMeshPool; }

      private:
        std::shared_ptr<Instance>       mInstance;
        std::shared_ptr<Surface>        mSurface;
        std::shared_ptr<PhysicalDevice> mPhysicalDevice;
        std::shared_ptr<Device>         mDevice;
        std::shared_ptr<SwapChain>      mSwapChain;
        std::shared_ptr<RenderPass>     mRenderPass;
        std::shared_ptr<CommandPool>    mCommandPool;
        std::shared_ptr<MeshPool>       mMeshPool;

        std::vector<vk::Semaphore> mImageAvailableSemaphores;
        std::vector<vk::Semaphore> mRenderFinishedSemaphores;
        std::vector<vk::Fence>     mInFlightFences;

        vk::SampleCountFlagBits mSamples;

        vk::ResultValue<std::uint32_t> mImageIndexResult = { vk::Result::eSuccess, 0 };
        bool                           mVSync;

        std::shared_ptr<Buffer> mVertexBuffer;
        std::shared_ptr<Buffer> mIndexBuffer;

        std::vector<std::uint32_t> mModelMeshes;
        std::uint32_t              mCurrentFrameIndex;
    };

} // namespace FREYA_NAMESPACE
