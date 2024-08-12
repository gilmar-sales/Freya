#pragma once

namespace FREYA_NAMESPACE
{
    struct Texture
    {
        operator std::uint32_t() { return id; }

        std::uint32_t imageBufferIndex;

        std::uint32_t width;
        std::uint32_t height;

        std::uint32_t id;
    };

} // namespace FREYA_NAMESPACE