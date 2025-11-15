#pragma once

#include <compare>
#include <cstdint>

namespace FREYA_NAMESPACE
{
    struct Mesh
    {
        operator size_t() const { return id; }

        operator bool() const { return id != 0; }

        auto operator<=>(const Mesh& other) const { return id <=> other.id; }

        std::uint32_t vertexBufferIndex;
        std::uint32_t vertexBufferOffset;

        std::uint32_t indexBufferIndex;
        std::uint32_t indexBufferOffset;
        std::uint32_t indexCount;

        std::uint32_t id;
    };

} // namespace FREYA_NAMESPACE