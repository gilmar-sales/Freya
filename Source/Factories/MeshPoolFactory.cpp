#include "Freya/Factories/MeshPoolFactory.hpp"

namespace FREYA_NAMESPACE
{
    Ref<MeshPool> MeshPoolFactory::CreateMeshPool()
    {
        return MakeRef<MeshPool>(mDevice, mPhysicalDevice, mCommandPool);
    }
} // namespace FREYA_NAMESPACE