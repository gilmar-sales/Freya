#include "Factories/MeshPoolFactory.hpp"

namespace FREYA_NAMESPACE 
{
    std::shared_ptr<MeshPool> MeshPoolFactory::CreateMeshPool()
    {
        return std::make_shared<MeshPool>(mDevice, mPhysicalDevice, mCommandPool);
    }
}