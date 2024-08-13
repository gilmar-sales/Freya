#include "Factories/MeshPoolFactory.hpp"

namespace FREYA_NAMESPACE 
{
    Ref<MeshPool> MeshPoolFactory::CreateMeshPool()
    {
        return std::make_shared<MeshPool>(mDevice, mPhysicalDevice, mCommandPool);
    }
}