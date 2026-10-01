#include "Freya/Core/UniformBuffer.hpp"

#include <gtest/gtest.h>

TEST(UniformBuffer, Stay256ByteAlignedForUboRings)
{
    static_assert(sizeof(fra::ProjectionUniformBuffer) % 256 == 0);
    static_assert(sizeof(fra::LightUniformBuffer) % 256 == 0);
    static_assert(sizeof(fra::ShadowUniformBuffer) % 256 == 0);
    static_assert(fra::MAX_LIGHTS == fra::kMaxLights);
    static_assert(fra::MAX_SHADOW_CASCADES == 4u);
}

TEST(UniformBuffer, ShadowMemberOffsetsMatchGlslStd140)
{
    static_assert(offsetof(fra::ShadowUniformBuffer, cascadeViewProj) == 0);
    static_assert(offsetof(fra::ShadowUniformBuffer, cascadeSplits) == 256);
    static_assert(offsetof(fra::ShadowUniformBuffer, params) == 272);
    static_assert(offsetof(fra::ShadowUniformBuffer, spotViewProj) == 288);
    static_assert(offsetof(fra::ShadowUniformBuffer, spotLightIndex) == 544);
    static_assert(offsetof(fra::ShadowUniformBuffer, pointLightPosFar) == 560);
    static_assert(offsetof(fra::ShadowUniformBuffer, pointFaceViewProj) == 656);
}
