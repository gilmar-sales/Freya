#pragma once

#include "Freya/Config.hpp"
#include "Freya/Core/Flags.hpp"

#include <cstdint>

namespace FREYA_NAMESPACE
{
    enum class SceneInstanceFlags : std::uint32_t
    {
        None        = 0,
        CastShadows = 1u,
        Translucent = 2u,
        Skinned     = 4u
    };

    enum class MaterialFlags : std::uint32_t
    {
        None          = 0,
        PackedMR      = 1u,
        Unlit         = 2u,
        DoubleSided   = 4u,
        ReceiveShadow = 8u
    };

    enum class CullFlags : std::uint32_t
    {
        None       = 0,
        ReverseZ   = 1u,
        HizEnabled = 2u
    };

    static_assert(sizeof(SceneInstanceFlags) == 4);
    static_assert(sizeof(MaterialFlags) == 4);
    static_assert(sizeof(CullFlags) == 4);

} // namespace FREYA_NAMESPACE
