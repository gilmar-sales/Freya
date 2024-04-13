#include "Builders/RendererBuilder.hpp"

#include "Builders/CommandPoolBuilder.hpp"
#include "Builders/DeviceBuilder.hpp"
#include "Builders/PhysicalDeviceBuilder.hpp"
#include "Builders/RenderPassBuilder.hpp"
#include "Builders/SurfaceBuilder.hpp"
#include "Builders/SwapChainBuilder.hpp"

namespace FREYA_NAMESPACE
{

    std::shared_ptr<Renderer> RendererBuilder::Build()
    {
        auto physicalDevice = PhysicalDeviceBuilder().SetInstance(mInstance).Build();

        auto surface = SurfaceBuilder()
                           .SetInstance(mInstance)
                           .SetPhysicalDevice(physicalDevice)
                           .SetWindow(mWindow)
                           .Build();

        mFrameCount = surface->QueryFrameCountSupport(mFrameCount);

        auto device = DeviceBuilder()
                          .SetInstance(mInstance)
                          .SetPhysicalDevice(physicalDevice)
                          .SetSurface(surface)
                          .Build();

        mSamples = physicalDevice->QuerySamplesSupport(mSamples);

        auto renderPass = RenderPassBuilder()
                              .SetDevice(device)
                              .SetPhysicalDevice(physicalDevice)
                              .SetSurface(surface)
                              .SetSamples(mSamples)
                              .SetFrameCount(mFrameCount)
                              .Build();

        auto swapChain = SwapChainBuilder()
                             .SetInstance(mInstance)
                             .SetPhysicalDevice(physicalDevice)
                             .SetDevice(device)
                             .SetSurface(surface)
                             .SetRenderPass(renderPass)
                             .SetWidth(mWidth)
                             .SetHeight(mHeight)
                             .SetFrameCount(mFrameCount)
                             .SetVSync(mVSync)
                             .SetSamples(mSamples)
                             .Build();

        auto commandPool =
            CommandPoolBuilder().SetDevice(device).SetSwapChain(swapChain).Build();

        auto imageAvailableSemaphores = std::vector<vk::Semaphore>(mFrameCount);

        auto renderFinishedSemaphores = std::vector<vk::Semaphore>(mFrameCount);

        auto inFlightFences = std::vector<vk::Fence>(mFrameCount);

        auto semaphoreInfo = vk::SemaphoreCreateInfo();

        auto fenceInfo =
            vk::FenceCreateInfo().setFlags(vk::FenceCreateFlagBits::eSignaled);

        for (size_t i = 0; i < mFrameCount; i++)
        {
            imageAvailableSemaphores[i] = device->Get().createSemaphore(semaphoreInfo);
            renderFinishedSemaphores[i] = device->Get().createSemaphore(semaphoreInfo);
            inFlightFences[i]           = device->Get().createFence(fenceInfo);

            assert(imageAvailableSemaphores[i] && renderFinishedSemaphores[i] &&
                   inFlightFences[i] &&
                   "Failed to create synchronization objects for a frame");
        }

        return std::make_shared<Renderer>(mInstance,
                                          surface,
                                          physicalDevice,
                                          device,
                                          swapChain,
                                          renderPass,
                                          commandPool,
                                          imageAvailableSemaphores,
                                          renderFinishedSemaphores,
                                          inFlightFences,
                                          mVSync,
                                          mSamples);
    }

} // namespace FREYA_NAMESPACE
