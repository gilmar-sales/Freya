#pragma once

#include "Freya/Asset/GpuScene.hpp"
#include "Freya/Asset/Vertex.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace FREYA_NAMESPACE
{
    /**
     * @brief CPU-side mesh LOD generation (meshoptimizer).
     *
     * All LODs share the same vertex buffer (UVs / normals / material
     * unchanged). The cull compute picks a LOD from screen-space diameter
     * (`CullPushConstants::lodPixelRef` / `lodStep`).
     */
    struct MeshLodBuildOptions
    {
        /// When false, only LOD0 (source indices) is kept.
        bool enabled = true;

        /// Index-count targets relative to LOD0, for LOD1..LOD3.
        /// Clamped to `kMaxLodsPerMesh - 1` entries; values must be in (0, 1).
        std::array<float, kMaxLodsPerMesh - 1> ratios { 0.22f, 0.08f, 0.03f };

        /// meshopt target error (relative). Higher = more aggressive.
        float targetError = 0.04f;

        /// Skip auto LODs when LOD0 has fewer indices than this.
        std::uint32_t minSourceIndices = 768;

        /// UV attribute weight for simplifyWithAttributes (texture
        /// preservation).
        float uvWeight = 1.0f;

        /// Normal attribute weight (shading continuity).
        float normalWeight = 0.5f;
    };

    /**
     * @brief Build index buffers for LOD0..N sharing `vertices`.
     *
     * LOD0 is always a copy of `indices`. Further LODs are decimated with
     * meshoptimizer attribute-aware simplify (UV + normal weights).
     */
    [[nodiscard]] std::vector<std::vector<std::uint32_t>> BuildMeshLodIndexSets(
        std::span<const Vertex>        vertices,
        std::span<const std::uint32_t> indices,
        const MeshLodBuildOptions&     options = {});

} // namespace FREYA_NAMESPACE
