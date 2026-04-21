#pragma once

#include <compare>
#include <cstdint>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Mesh data structure with buffer indices and offsets.
     *
     * Contains references to vertex/index buffers and counts for drawing.
     * Implicitly converts to size_t for use as ID in SparseSet/MeshSet.
     *
     * @param vertexBufferIndex  Index into MeshPool's vertex buffer vector
     * @param vertexBufferOffset Byte offset into the vertex buffer
     * @param indexBufferIndex   Index into MeshPool's index buffer vector
     * @param indexBufferOffset  Byte offset into the index buffer
     * @param indexCount         Number of indices to draw
     * @param id                 Unique mesh identifier
     */
    struct Mesh
    {
        /**
         * @brief Conversion operator to mesh ID (size_t).
         */
        operator size_t() const { return id; }

        /**
         * @brief Conversion operator to boolean (valid if id != 0).
         */
        operator bool() const { return id != 0; }

        /**
         * @brief Spaceship operator for ordered comparison.
         */
        auto operator<=>(const Mesh& other) const { return id <=> other.id; }

        std::uint32_t vertexBufferIndex;  ///< Vertex buffer index
        std::uint32_t vertexBufferOffset; ///< Byte offset in vertex buffer

        std::uint32_t indexBufferIndex;  ///< Index buffer index
        std::uint32_t indexBufferOffset; ///< Byte offset in index buffer
        std::uint32_t indexCount;        ///< Number of indices

        std::uint32_t id; ///< Unique mesh identifier
    };

} // namespace FREYA_NAMESPACE