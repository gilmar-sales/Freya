#include <Freya/Core/Limits.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("runtime limits match shader/CPU contracts", "[limits]")
{
    STATIC_REQUIRE(fra::kMaxLights == 64u);
    STATIC_REQUIRE(fra::kMaxShadowCascades == 4u);
    STATIC_REQUIRE(fra::kMaxSpotShadows == 4u);
    STATIC_REQUIRE(fra::kMaxPointShadows == 2u);
    STATIC_REQUIRE(fra::kMaxMaterialSets == 1024u);
    STATIC_REQUIRE(fra::kGpuAnimMaxJoints == 128u);
    STATIC_REQUIRE(fra::kGpuAnimMaxInstances == 2048u);
    STATIC_REQUIRE(fra::kGpuAnimMaxClips == 24u);

    STATIC_REQUIRE(static_cast<std::uint32_t>(fra::LightType::Point) == 0u);
    STATIC_REQUIRE(
        static_cast<std::uint32_t>(fra::LightType::Directional) == 1u);
    STATIC_REQUIRE(static_cast<std::uint32_t>(fra::LightType::Spot) == 2u);
    STATIC_REQUIRE(static_cast<std::uint32_t>(fra::LightType::Area) == 3u);
}
