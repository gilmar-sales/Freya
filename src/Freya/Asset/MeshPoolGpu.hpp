#pragma once

/**
 * @file MeshPoolGpu.hpp
 * @brief Internal GPU mesh-table sync (IndirectDrawSystem). Not public API.
 */

#include "Freya/Asset/GpuScene.hpp"
#include "Freya/Asset/MeshPool.hpp"

#include <vector>

namespace FREYA_NAMESPACE
{
    class MeshPoolGpuAccess
    {
      public:
        static void FillMeshInfos(const MeshPool&        pool,
                                  std::vector<MeshInfo>& out);
        static void FillMeshLods(const MeshPool&           pool,
                                 std::vector<MeshLodInfo>& out);
    };

} // namespace FREYA_NAMESPACE
