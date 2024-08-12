#pragma once

namespace FREYA_NAMESPACE
{
    struct Mesh
    {
        operator std::uint32_t() { return id; }

        std::uint32_t vertexBufferIndex;
        std::uint32_t vertexBufferOffset;

        std::uint32_t indexBufferIndex;
        std::uint32_t indexBufferOffset;
        std::uint32_t indexCount;

        std::uint32_t id;
    };

} // namespace FREYA_NAMESPACE