#pragma once

#include "Freya/Asset/BoneMatrixResources.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/GpuAnimPass.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    class GpuAnimPassBuilder
    {
      public:
        GpuAnimPassBuilder(
            const skr::Arc<Device>&                          device,
            const skr::Arc<FreyaOptions>&                    freyaOptions,
            const skr::Arc<BoneMatrixResources>&             boneResources,
            const skr::Arc<skr::ServiceProvider>&            serviceProvider,
            const skr::Arc<skr::Logger<GpuAnimPassBuilder>>& logger) :
            mDevice(device), mFreyaOptions(freyaOptions),
            mBoneResources(boneResources), mServiceProvider(serviceProvider),
            mLogger(logger)
        {
        }

        skr::Arc<GpuAnimPass> Build();

      private:
        skr::Arc<Device>                          mDevice;
        skr::Arc<FreyaOptions>                    mFreyaOptions;
        skr::Arc<BoneMatrixResources>             mBoneResources;
        skr::Arc<skr::ServiceProvider>            mServiceProvider;
        skr::Arc<skr::Logger<GpuAnimPassBuilder>> mLogger;
    };

} // namespace FREYA_NAMESPACE
