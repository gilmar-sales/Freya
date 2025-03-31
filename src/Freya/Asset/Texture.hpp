#pragma once

#include "Freya/Core/Image.hpp"

namespace FREYA_NAMESPACE
{
    struct Texture
    {
        operator std::uint32_t() const { return id; }

        Ref<Image>  image;
        vk::Sampler sampler;

        std::uint32_t width;
        std::uint32_t height;

        std::uint32_t id;
    };

} // namespace FREYA_NAMESPACE