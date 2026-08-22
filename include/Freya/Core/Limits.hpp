#pragma once

#include "Freya/Config.hpp"

#include <cstdint>

namespace FREYA_NAMESPACE
{
    constexpr std::uint32_t kMaxLights         = 64;
    constexpr std::uint32_t kMaxShadowCascades = 4;
    constexpr std::uint32_t kMaxSpotShadows    = 4;
    constexpr std::uint32_t kMaxPointShadows   = 2;
    constexpr std::uint32_t kMaxMaterialSets   = 1024;

    constexpr std::uint32_t kGpuAnimMaxJoints    = 128;
    constexpr std::uint32_t kGpuAnimMaxInstances = 2048;
    constexpr std::uint32_t kGpuAnimMaxClips     = 24;

    enum class LightType : std::uint32_t
    {
        Point       = 0,
        Directional = 1,
        Spot        = 2,
        Area        = 3,
    };

} // namespace FREYA_NAMESPACE
