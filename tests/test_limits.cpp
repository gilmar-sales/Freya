#include <Freya/Core/Limits.hpp>

#include <gtest/gtest.h>

TEST(Limits, RuntimeLimitsMatchShaderCpuContracts)
{
    static_assert(fra::kMaxLights == 64u);
    static_assert(fra::kMaxShadowCascades == 4u);
    static_assert(fra::kMaxSpotShadows == 4u);
    static_assert(fra::kMaxPointShadows == 2u);
    static_assert(fra::kMaxMaterialSets == 1024u);
    static_assert(fra::kGpuAnimMaxJoints == 128u);
    static_assert(fra::kGpuAnimMaxInstances == 2048u);
    static_assert(fra::kGpuAnimMaxClips == 24u);

    static_assert(static_cast<std::uint32_t>(fra::LightType::Point) == 0u);
    static_assert(static_cast<std::uint32_t>(fra::LightType::Directional) ==
                  1u);
    static_assert(static_cast<std::uint32_t>(fra::LightType::Spot) == 2u);
    static_assert(static_cast<std::uint32_t>(fra::LightType::Area) == 3u);
}
