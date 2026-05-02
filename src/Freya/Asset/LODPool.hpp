#pragma once

#include "Freya/Asset/LODTypes.hpp"
#include "Freya/Containers/SparseSet.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/CommandPool.hpp"

namespace FREYA_NAMESPACE
{

class MeshPool;

/**
 * @brief Manages LOD groups - collections of meshes representing the same geometry at different detail levels
 *
 * LODPool creates and maintains groups of LOD levels for mesh assets.
 * Each group contains references to meshes in MeshPool at different detail levels.
 */
class LODPool
{
  public:
    LODPool(const Ref<Device>&                device,
            const Ref<CommandPool>&           commandPool,
            const Ref<MeshPool>&              meshPool,
            const Ref<skr::Logger<LODPool>>& logger);

    ~LODPool();

    /**
     * @brief Creates a new LOD group from existing meshes
     * @param meshIds Array of mesh IDs (LOD0, LOD1, LOD2, ...)
     * @param distances Array of switch distances for each level
     * @return Group ID for use with LODService
     */
    std::uint32_t CreateLODGroup(std::span<std::uint32_t> meshIds,
                                  std::span<float>        distances);

    /**
     * @brief Creates an LOD group automatically by simplifying a base mesh
     * @param baseMeshId The high-polygon mesh to generate LODs from
     * @param config LOD generation configuration
     * @return Group ID for use with LODService
     */
    std::uint32_t CreateLODGroupFromMesh(std::uint32_t       baseMeshId,
                                          const LODConfig&   config);

    /**
     * @brief Gets the mesh ID for a specific LOD level
     * @param groupId LOD group ID
     * @param level LOD level (0 = highest detail)
     * @return Mesh ID from MeshPool
     */
    [[nodiscard]] std::uint32_t GetLODMesh(std::uint32_t groupId,
                                            std::uint32_t level) const;

    /**
     * @brief Gets the total number of LOD levels in a group
     */
    [[nodiscard]] std::uint32_t GetLevelCount(std::uint32_t groupId) const;

    /**
     * @brief Returns flat array of all LOD levels for GPU buffer upload
     */
    [[nodiscard]] std::span<const LODLevel> GetAllLevels() const { return mFlatLevels; }

    /**
     * @brief Gets the group offset in the flat levels array
     */
    [[nodiscard]] std::uint32_t GetGroupOffset(std::uint32_t groupId) const;

        /**
         * @brief Gets the level offset within its group's slice of the flat array
         */
        std::uint32_t GetLevelOffset(std::uint32_t groupId,
                                     std::uint32_t level) const;

        /**
         * @brief Returns the number of LOD groups.
         */
        [[nodiscard]] std::uint32_t GetGroupCount() const { return static_cast<std::uint32_t>(mGroups.size()); }

  private:
    Ref<Device>                mDevice;
    Ref<CommandPool>           mCommandPool;
    Ref<MeshPool>              mMeshPool;
    Ref<skr::Logger<LODPool>> mLogger;

    // Dense set of LOD groups
    std::vector<LODGroup> mGroups;

    // Flat array of all LOD levels (for efficient GPU buffer)
    // Layout: [group0_lod0, group0_lod1, ..., group1_lod0, ...]
    std::vector<LODLevel> mFlatLevels;

    // Maps group ID to its slice in mFlatLevels
    std::vector<std::uint32_t> mGroupOffsets;
};

} // namespace FREYA_NAMESPACE