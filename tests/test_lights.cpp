#include <Freya/Core/LightService.hpp>
#include <Freya/Core/Limits.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

TEST_CASE("light factories pack LightType into type field", "[lights]")
{
    const auto point =
        fra::MakePointLight({ 1.f, 2.f, 3.f }, { 1.f, 0.f, 0.f }, 8.f, 2.5f);
    REQUIRE(point.type == static_cast<float>(fra::LightType::Point));
    REQUIRE(point.position == glm::vec3(1.f, 2.f, 3.f));
    REQUIRE(point.radius == Catch::Approx(8.f));
    REQUIRE(point.intensity == Catch::Approx(2.5f));

    const auto dir =
        fra::MakeDirectionalLight({ 0.f, -2.f, 0.f }, { 1.f, 1.f, 1.f }, 1.f);
    REQUIRE(dir.type == static_cast<float>(fra::LightType::Directional));
    REQUIRE(glm::length(dir.direction) == Catch::Approx(1.f));

    const auto spot = fra::MakeSpotLight(
        { 0.f, 1.f, 0.f }, { 0.f, -1.f, 0.f }, { 1.f, 1.f, 1.f }, 12.f,
        glm::radians(15.f), glm::radians(30.f), 4.f);
    REQUIRE(spot.type == static_cast<float>(fra::LightType::Spot));
    REQUIRE(spot.innerCutoff == Catch::Approx(std::cos(glm::radians(15.f))));
    REQUIRE(spot.outerCutoff == Catch::Approx(std::cos(glm::radians(30.f))));

    const auto area =
        fra::MakeAreaLight({ 0.f, 2.f, 0.f }, { 0.f, -1.f, 0.f },
                           { 1.f, 0.f, 0.f }, 0.5f, 0.25f, { 1.f, 1.f, 1.f });
    REQUIRE(area.type == static_cast<float>(fra::LightType::Area));
    REQUIRE(area.outerCutoff == Catch::Approx(0.5f));
    REQUIRE(area.halfHeight == Catch::Approx(0.25f));
    REQUIRE(glm::dot(area.direction, area.tangent) ==
            Catch::Approx(0.f).margin(1e-5f));
}
