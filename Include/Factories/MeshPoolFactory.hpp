#pragma once

#include "Asset/MeshPool.hpp"

namespace FREYA_NAMESPACE
{

    class MeshPoolFactory
    {
      public:
        MeshPoolFactory(const Ref<Device>&         device,
                        const Ref<PhysicalDevice>& physicalDevice,
                        const Ref<CommandPool>&    commandPool) :
            mDevice(device), mPhysicalDevice(physicalDevice), mCommandPool(commandPool)
        {
        }

        Ref<MeshPool> CreateMeshPool();

      private:
        Ref<Device>         mDevice;
        Ref<PhysicalDevice> mPhysicalDevice;
        Ref<CommandPool>    mCommandPool;
    };
} // namespace FREYA_NAMESPACE