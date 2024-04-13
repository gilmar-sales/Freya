#pragma once

#include "Asset/MeshPool.hpp"

namespace FREYA_NAMESPACE
{

    class MeshPoolFactory
    {
        public:
        MeshPoolFactory(std::shared_ptr<Device> device,
                 std::shared_ptr<PhysicalDevice> physicalDevice,
                 std::shared_ptr<CommandPool> commandPool) :
                 mDevice(device), mPhysicalDevice(physicalDevice), mCommandPool(commandPool)
                 {

                 }

        std::shared_ptr<MeshPool> CreateMeshPool();

        private:
            std::shared_ptr<Device> mDevice;
            std::shared_ptr<PhysicalDevice> mPhysicalDevice;
            std::shared_ptr<CommandPool> mCommandPool;
    };
}