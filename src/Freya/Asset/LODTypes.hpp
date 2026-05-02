#pragma once

#include <cstdint>
#include <vector>
#include <span>

namespace FREYA_NAMESPACE
{

// Forward declarations
class Device;
class CommandPool;
class MeshPool;

/**
 * @brief Metadata for a single LOD level within an LOD group
 *
 * Each LOD level specifies which mesh to render at what distance range.
 * Levels are ordered from highest detail (LOD 0) to lowest.
 */
struct alignas(16) LODLevel {
    std::uint32_t meshId;          // Reference to MeshPool mesh
    float         switchDistance;   // Camera distance (squared) to switch TO this LOD
    float         fadeStart;        // Start distance for dither cross-fade
    std::uint32_t padding;         // Explicit 16-byte stride matching GLSL layout in LODSelection.comp

    bool operator<(const LODLevel& other) const {
        return switchDistance < other.switchDistance;
    }
};

static_assert(sizeof(LODLevel) == 16,
              "LODLevel must be 16 bytes (alignas(16) + members + padding) to match GLSL struct layout");

/**
 * @brief A group of LOD levels representing the same geometry at different detail levels
 *
 * An LODGroup is created for each unique mesh asset, containing the high-poly
 * base mesh and progressively simplified versions.
 */
struct LODGroup {
    std::vector<LODLevel>  levels;       // Ordered by detail (0 = highest)
    std::uint32_t          baseMeshId;    // Original high-polygon mesh ID
    float                  boundsRadius;  // Precomputed bounding sphere radius
    glm::vec3              boundsCenter; // Precomputed bounding sphere center

    std::uint32_t GetLevelCount() const { return static_cast<std::uint32_t>(levels.size()); }
};

/**
 * @brief GPU-visible per-instance LOD data (Structure of Arrays layout)
 *
 * This buffer is uploaded to GPU and read by the compute shader for
 * LOD selection and culling.
 */
struct LODInstanceData {
    std::uint32_t meshGroupId;    // Which LODGroup this instance uses
    std::uint32_t currentLOD;      // Currently selected LOD level (updated by GPU)
    std::uint32_t transformIndex;  // Index into transform storage buffer
    float         padding;        // Alignment padding
};

/**
 * @brief Indirect draw command for multi-draw indirect (MDI)
 *
 * Matches VkDrawIndexedIndirectCommand layout exactly.
 */
struct DrawIndexedIndirectCommand {
    std::uint32_t indexCount;
    std::uint32_t instanceCount;
    std::uint32_t firstIndex;
    std::int32_t  vertexOffset;
    std::uint32_t firstInstance;
};

/**
 * @brief Push constants for LOD compute shader
 */
struct LODPushConstants {
    glm::vec3    cameraPosition;        // Camera world position for distance calculation
    float        globalDrawDistance;   // Maximum draw distance (squared)
    std::uint32_t instanceCount;       // Total number of LOD instances to process
    std::uint32_t lodLevelCount;       // Number of LOD levels per group (not total across groups)
};

/**
 * @brief Configuration for automatic LOD generation
 */
struct LODConfig {
    std::vector<float> distances = { 0.0f, 50.0f, 150.0f, 400.0f };
    bool                useDitherFade = true;
    std::uint32_t      maxLODLevels = 4;
    float              ditherFadeStart = 10.0f; // Distance before switch to start fade
};

/**
 * @brief GPU metadata for a mesh used by compute shader
 *
 * Layout must match the MeshMetadata struct in LODSelection.comp exactly.
 */
struct MeshMetadata {
    std::uint32_t indexCount;    // Number of indices to draw
    std::uint32_t firstIndex;    // First index offset in index buffer
    std::int32_t  vertexOffset; // Base vertex offset
    std::uint32_t padding;      // Alignment padding
};

} // namespace FREYA_NAMESPACE