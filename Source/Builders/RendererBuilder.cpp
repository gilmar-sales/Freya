#include "Builders/RendererBuilder.hpp"

#include "Builders/CommandPoolBuilder.hpp"
#include "Builders/DeviceBuilder.hpp"
#include "Builders/ForwardPassBuilder.hpp"
#include "Builders/PhysicalDeviceBuilder.hpp"
#include "Builders/SurfaceBuilder.hpp"
#include "Builders/SwapChainBuilder.hpp"

namespace FREYA_NAMESPACE
{

    Ref<Renderer> RendererBuilder::Build()
    {
        auto instance = mInstanceBuilder.Build();

        assert(instance && "Failed to create fra::Instance");

        auto physicalDevice = PhysicalDeviceBuilder().SetInstance(instance).Build();

        auto surface = SurfaceBuilder()
                           .SetInstance(instance)
                           .SetPhysicalDevice(physicalDevice)
                           .SetWindow(mWindow)
                           .SetWidth(mWidth)
                           .SetHeight(mHeight)
                           .Build();

        mFrameCount = surface->QueryFrameCountSupport(mFrameCount);

        auto device = DeviceBuilder()
                          .SetInstance(instance)
                          .SetPhysicalDevice(physicalDevice)
                          .SetSurface(surface)
                          .Build();

        mSamples = physicalDevice->QuerySamplesSupport(mSamples);

        auto renderPass = ForwardPassBuilder()
                              .SetDevice(device)
                              .SetPhysicalDevice(physicalDevice)
                              .SetSurface(surface)
                              .SetSamples(mSamples)
                              .SetFrameCount(mFrameCount)
                              .Build();

        auto swapChain = SwapChainBuilder()
                             .SetInstance(instance)
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

        auto commandPool = CommandPoolBuilder()
                               .SetDevice(device)
                               .SetCount(mFrameCount)
                               .Build();

        auto imageAvailableSemaphores = std::vector<vk::Semaphore>(mFrameCount);

        auto renderFinishedSemaphores = std::vector<vk::Semaphore>(mFrameCount);

        auto inFlightFences = std::vector<vk::Fence>(mFrameCount);

        constexpr auto semaphoreInfo = vk::SemaphoreCreateInfo();

        constexpr auto fenceInfo =
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

        return MakeRef<Renderer>(instance,
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
                                 mSamples,
                                 mClearColor,
                                 mDrawDistance,
                                 mEventManager);
    }

} // namespace FREYA_NAMESPACE
