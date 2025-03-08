#include "Freya/Builders/RendererBuilder.hpp"

#include "Freya/Builders/CommandPoolBuilder.hpp"
#include "Freya/Builders/DeviceBuilder.hpp"
#include "Freya/Builders/ForwardPassBuilder.hpp"
#include "Freya/Builders/PhysicalDeviceBuilder.hpp"
#include "Freya/Builders/SurfaceBuilder.hpp"
#include "Freya/Builders/SwapChainBuilder.hpp"

namespace FREYA_NAMESPACE
{

    Ref<Renderer> RendererBuilder::Build()
    {
        auto instance = mInstanceBuilder.Build();

        assert(instance && "Failed to create fra::Instance");

        auto physicalDevice =
            PhysicalDeviceBuilder().SetInstance(instance).Build();

        auto surface =
            SurfaceBuilder()
                .SetInstance(instance)
                .SetPhysicalDevice(physicalDevice)
                .SetWindow(mWindow)
                .Build();

        mFrameCount = surface->QueryFrameCountSupport(mFrameCount);

        auto device = DeviceBuilder()
                          .SetInstance(instance)
                          .SetPhysicalDevice(physicalDevice)
                          .SetSurface(surface)
                          .Build();

        mSamples = physicalDevice->QuerySamplesSupport(mSamples);

        auto renderPass =
            ForwardPassBuilder()
                .SetDevice(device)
                .SetPhysicalDevice(physicalDevice)
                .SetSurface(surface)
                .SetSamples(mSamples)
                .SetFrameCount(mFrameCount)
                .Build();

        auto swapChain =
            SwapChainBuilder()
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

        auto commandPool =
            CommandPoolBuilder()
                .SetDevice(device)
                .SetCount(mFrameCount)
                .Build();

        return MakeRef<Renderer>(
            instance,
            surface,
            physicalDevice,
            device,
            swapChain,
            renderPass,
            commandPool,
            mVSync,
            mSamples,
            mClearColor,
            mDrawDistance,
            mEventManager);
    }

} // namespace FREYA_NAMESPACE
