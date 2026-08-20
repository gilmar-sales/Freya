#include "Freya/Core/UniformBuffer.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("uniform buffers stay 256-byte aligned for UBO rings", "[ubo]")
{
    STATIC_REQUIRE(sizeof(fra::ProjectionUniformBuffer) % 256 == 0);
    STATIC_REQUIRE(sizeof(fra::LightUniformBuffer) % 256 == 0);
    STATIC_REQUIRE(sizeof(fra::ShadowUniformBuffer) % 256 == 0);
    STATIC_REQUIRE(fra::MAX_LIGHTS == fra::kMaxLights);
    STATIC_REQUIRE(fra::MAX_SHADOW_CASCADES == 4u);
}

TEST_CASE("shadow UBO member offsets match GLSL std140", "[ubo]")
{
    STATIC_REQUIRE(offsetof(fra::ShadowUniformBuffer, cascadeViewProj) == 0);
    STATIC_REQUIRE(offsetof(fra::ShadowUniformBuffer, cascadeSplits) == 256);
    STATIC_REQUIRE(offsetof(fra::ShadowUniformBuffer, params) == 272);
    STATIC_REQUIRE(offsetof(fra::ShadowUniformBuffer, spotViewProj) == 288);
    STATIC_REQUIRE(offsetof(fra::ShadowUniformBuffer, spotLightIndex) == 544);
    STATIC_REQUIRE(offsetof(fra::ShadowUniformBuffer, pointLightPosFar) == 560);
}
