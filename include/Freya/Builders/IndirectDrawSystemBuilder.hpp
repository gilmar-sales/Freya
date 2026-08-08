#pragma once

#include "Freya/Core/IndirectDrawSystem.hpp"
#include "Freya/Core/PhysicalDevice.hpp"
#include "Freya/FreyaOptions.hpp"

namespace FREYA_NAMESPACE
{
    class IndirectDrawSystemBuilder
    {
      public:
        IndirectDrawSystemBuilder(
            const skr::Arc<Device>&                      device,
            const skr::Arc<PhysicalDevice>&              physicalDevice,
            const skr::Arc<CommandPool>&                 commandPool,
            const skr::Arc<MeshPool>&                    meshPool,
            const skr::Arc<MaterialDescriptorResources>& materials,
            const skr::Arc<FreyaOptions>&                freyaOptions,
            const skr::Arc<skr::ServiceProvider>&        serviceProvider);

        skr::Arc<IndirectDrawSystem> Build();

      private:
        skr::Arc<Device>                      mDevice;
        skr::Arc<PhysicalDevice>              mPhysicalDevice;
        skr::Arc<CommandPool>                 mCommandPool;
        skr::Arc<MeshPool>                    mMeshPool;
        skr::Arc<MaterialDescriptorResources> mMaterials;
        skr::Arc<FreyaOptions>                mFreyaOptions;
        skr::Arc<skr::ServiceProvider>        mServiceProvider;
    };
} // namespace FREYA_NAMESPACE
