#pragma once

#include <compare>
#include <cstdint>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Mesh record in the global mega geometry buffers.
     *
     * All meshes share one vertex and one uint32 index buffer. LOD ranges are
     * indexed via `lodBase` / `lodCount` into MeshPool's LOD table.
     */
    struct Mesh
    {
        operator size_t() const { return id; }

        operator bool() const { return id != 0; }

        auto operator<=>(const Mesh& other) const { return id <=> other.id; }

        std::uint32_t vertexBufferOffset = 0; ///< Byte offset in mega VBO
        std::uint32_t indexBufferOffset  = 0; ///< Byte offset of LOD0 indices
        std::uint32_t firstIndex         = 0; ///< LOD0 index units for MDI
        std::int32_t  vertexOffset       = 0; ///< Vertex units for MDI
        std::uint32_t indexCount         = 0; ///< LOD0 index count
        std::uint32_t lodCount           = 1;
        std::uint32_t lodBase            = 0; ///< Index into MeshLodInfo table

        glm::vec3 aabbMin { 0.0f };
        glm::vec3 aabbMax { 0.0f };
        /// True when AABBs were inflated for skinned cull conservatism.
        bool skinned = false;

        std::uint32_t id = 0;
    };

} // namespace FREYA_NAMESPACE
