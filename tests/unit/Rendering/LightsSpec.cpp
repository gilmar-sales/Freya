#include <Freya/Core/LightService.hpp>
#include <Freya/Core/Limits.hpp>

#include <gtest/gtest.h>

#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

TEST(Lights, FactoriesPackLightTypeIntoTypeField)
{
    const auto point =
        fra::MakePointLight({ 1.f, 2.f, 3.f }, { 1.f, 0.f, 0.f }, 8.f, 2.5f);
    EXPECT_EQ(point.type, fra::LightType::Point);
    EXPECT_TRUE(point.position == glm::vec3(1.f, 2.f, 3.f));
    EXPECT_FLOAT_EQ(point.radius, 8.f);
    EXPECT_FLOAT_EQ(point.intensity, 2.5f);

    const auto dir =
        fra::MakeDirectionalLight({ 0.f, -2.f, 0.f }, { 1.f, 1.f, 1.f }, 1.f);
    EXPECT_EQ(dir.type, fra::LightType::Directional);
    EXPECT_FLOAT_EQ(glm::length(dir.direction), 1.f);

    const auto spot = fra::MakeSpotLight(
        { 0.f, 1.f, 0.f }, { 0.f, -1.f, 0.f }, { 1.f, 1.f, 1.f }, 12.f,
        glm::radians(15.f), glm::radians(30.f), 4.f);
    EXPECT_EQ(spot.type, fra::LightType::Spot);
    EXPECT_NEAR(spot.innerCutoff, std::cos(glm::radians(15.f)), 1e-6);
    EXPECT_NEAR(spot.outerCutoff, std::cos(glm::radians(30.f)), 1e-6);

    const auto area =
        fra::MakeAreaLight({ 0.f, 2.f, 0.f }, { 0.f, -1.f, 0.f },
                           { 1.f, 0.f, 0.f }, 0.5f, 0.25f, { 1.f, 1.f, 1.f });
    EXPECT_EQ(area.type, fra::LightType::Area);
    EXPECT_FLOAT_EQ(area.outerCutoff, 0.5f);
    EXPECT_FLOAT_EQ(area.halfHeight, 0.25f);
    EXPECT_NEAR(glm::dot(area.direction, area.tangent), 0.0, 1e-5);
}

TEST(Lights, UploadDefaultsToNullHandle)
{
    const fra::LightUpload upload {};
    EXPECT_FALSE(upload.handle.IsValid());
    EXPECT_EQ(upload.light.type, fra::LightType::Point);
}

TEST(Lights, DefaultsToEnabled)
{
    const fra::Light light {};
    EXPECT_TRUE(fra::HasFlag(light.flags, fra::LightFlags::Enabled));
    EXPECT_TRUE(fra::HasFlag(light.flags, fra::LightFlags::CastShadows));

    auto muted = fra::MakePointLight({ 0.f, 1.f, 0.f }, { 1.f, 1.f, 1.f }, 5.f);
    fra::SetFlag(muted.flags, fra::LightFlags::Enabled, false);
    EXPECT_FALSE(fra::HasFlag(muted.flags, fra::LightFlags::Enabled));
}
