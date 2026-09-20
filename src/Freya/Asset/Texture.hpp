#pragma once

#include "Freya/Core/Image.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Texture structure with image, sampler, and dimensions.
     *
     * @param image   Image reference
     * @param sampler Vulkan sampler handle
     * @param width   Texture width in pixels
     * @param height  Texture height in pixels
     * @param id      Unique texture identifier
     */
    struct Texture
    {
        /**
         * @brief Conversion operator to texture ID.
         */
        operator std::uint32_t() const { return id; }

        skr::Arc<Image> image;   ///< Vulkan image and memory (null if external)
        vk::Sampler     sampler; ///< Sampler state (filtering, addressing)
        std::uint32_t   width;   ///< Width in pixels
        std::uint32_t   height;  ///< Height in pixels
        std::uint32_t   id;      ///< Unique texture identifier
        bool external = false;   ///< View/sampler owned elsewhere (e.g. RT)
        vk::ImageView externalView {}; ///< Valid when external
    };

} // namespace FREYA_NAMESPACE