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
        auto instanceBuilder = mServiceProvider->GetService<InstanceBuilder>();

        mInstanceBuilderFunc(*instanceBuilder);

        auto instance = instanceBuilder->Build();

        mLogger->Assert(instance != nullptr, "Failed to create fra::Instance");

        auto physicalDevice =
            mServiceProvider->GetService<PhysicalDeviceBuilder>()
                ->SetInstance(instance)
                .Build();

        auto surface =
            mServiceProvider->GetService<SurfaceBuilder>()
                ->SetInstance(instance)
                .SetPhysicalDevice(physicalDevice)
                .SetWindow(mWindow)
                .Build();

        mFrameCount = surface->QueryFrameCountSupport(mFrameCount);

        auto device = mServiceProvider->GetService<DeviceBuilder>()
                          ->SetInstance(instance)
                          .SetPhysicalDevice(physicalDevice)
                          .SetSurface(surface)
                          .Build();

        mSamples = physicalDevice->QuerySamplesSupport(mSamples);

        auto renderPass =
            mServiceProvider->GetService<ForwardPassBuilder>()
                ->SetDevice(device)
                .SetPhysicalDevice(physicalDevice)
                .SetSurface(surface)
                .SetSamples(mSamples)
                .SetFrameCount(mFrameCount)
                .Build();

        auto swapChain =
            mServiceProvider->GetService<SwapChainBuilder>()
                ->SetInstance(instance)
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

        mLogger->LogTrace("Creating renderer - Frame count: {} - Samples: {}",
                          mFrameCount,
                          static_cast<int>(mSamples));
        return MakeRef<Renderer>(
            instance,
            surface,
            physicalDevice,
            device,
            swapChain,
            renderPass,
            commandPool,
            mServiceProvider,
            mVSync,
            mSamples,
            mClearColor,
            mDrawDistance,
            mEventManager);
    }

} // namespace FREYA_NAMESPACE
