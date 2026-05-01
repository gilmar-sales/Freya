#include "Freya/Asset/LODPool.hpp"
#include "Freya/Asset/MeshPool.hpp"

namespace FREYA_NAMESPACE
{

    LODPool::LODPool(const Ref<Device>&               device,
                     const Ref<CommandPool>&          commandPool,
                     const Ref<MeshPool>&             meshPool,
                     const Ref<skr::Logger<LODPool>>& logger) :
        mDevice(device), mCommandPool(commandPool), mMeshPool(meshPool),
        mLogger(logger)
    {
        mLogger->LogInformation("LODPool initialized");
    }

    LODPool::~LODPool() = default;

    std::uint32_t LODPool::CreateLODGroup(std::span<std::uint32_t> meshIds,
                                          std::span<float>         distances)
    {
        if (meshIds.size() != distances.size())
        {
            mLogger->LogError("LODPool::CreateLODGroup: meshIds and distances "
                              "size mismatch ({} vs {})",
                              meshIds.size(),
                              distances.size());
            return std::numeric_limits<std::uint32_t>::max();
        }

        const std::uint32_t groupId =
            static_cast<std::uint32_t>(mGroups.size());

        LODGroup group;
        group.baseMeshId   = meshIds[0];
        group.boundsRadius = 0.0f; // TODO: Compute from mesh geometry
        group.boundsCenter = glm::vec3(0.0f);

        // Create LOD levels
        for (std::size_t i = 0; i < meshIds.size(); ++i)
        {
            LODLevel level;
            level.meshId         = meshIds[i];
            level.switchDistance = distances[i];
            level.fadeStart =
                distances[i] * 0.8f; // Fade starts at 80% of switch distance
            group.levels.push_back(level);
        }

        // Sort by switch distance (ascending)
        std::sort(group.levels.begin(), group.levels.end());

        mGroups.push_back(group);

        // Record offset in flat array
        const std::uint32_t offset =
            static_cast<std::uint32_t>(mFlatLevels.size());
        mGroupOffsets.push_back(offset);

        // Append levels to flat array
        for (const auto& level : group.levels)
        {
            mFlatLevels.push_back(level);
        }

        mLogger->LogInformation("Created LOD group {} with {} levels", groupId,
                                group.levels.size());
        return groupId;
    }

    std::uint32_t LODPool::CreateLODGroupFromMesh(std::uint32_t    baseMeshId,
                                                  const LODConfig& config)
    {
        mLogger->LogWarning(
            "LODPool::CreateLODGroupFromMesh: Automatic LOD generation "
            "not yet implemented. Use CreateLODGroup with pre-generated "
            "meshes.");

        // TODO: Implement automatic LOD generation via mesh decimation
        // For now, return invalid group ID
        return std::numeric_limits<std::uint32_t>::max();
    }

    std::uint32_t LODPool::GetLODMesh(std::uint32_t groupId,
                                      std::uint32_t level) const
    {
        if (groupId >= mGroups.size())
        {
            mLogger->LogError("LODPool::GetLODMesh: Invalid group ID {}",
                              groupId);
            return std::numeric_limits<std::uint32_t>::max();
        }

        const auto& group = mGroups[groupId];
        if (level >= group.levels.size())
        {
            mLogger->LogError(
                "LODPool::GetLODMesh: Invalid level {} for group {}", level,
                groupId);
            return std::numeric_limits<std::uint32_t>::max();
        }

        return group.levels[level].meshId;
    }

    std::uint32_t LODPool::GetLevelCount(std::uint32_t groupId) const
    {
        if (groupId >= mGroups.size())
        {
            return 0;
        }
        return static_cast<std::uint32_t>(mGroups[groupId].levels.size());
    }

    std::uint32_t LODPool::GetGroupOffset(std::uint32_t groupId) const
    {
        if (groupId >= mGroupOffsets.size())
        {
            return 0;
        }
        return mGroupOffsets[groupId];
    }

    std::uint32_t LODPool::GetLevelOffset(std::uint32_t groupId,
                                          std::uint32_t level) const
    {
        if (groupId >= mGroups.size())
        {
            return 0;
        }

        const auto& group = mGroups[groupId];
        if (level >= group.levels.size())
        {
            return 0;
        }

        return mGroupOffsets[groupId] + level;
    }

} // namespace FREYA_NAMESPACE