#include <Freya/Asset/LightingTechniqueRegistry.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("LightingTechniqueRegistry defaults and override",
          "[lighting-technique]")
{
    fra::LightingTechniqueRegistry registry;

    REQUIRE_FALSE(registry.HasOverride());
    REQUIRE(registry.Fragment().empty());
    REQUIRE(registry.FragmentOrDefault() ==
            fra::LightingTechniqueRegistry::kDefaultFragment);

    registry.SetFragment("Cell/lighting_cell.frag.spv");
    REQUIRE(registry.HasOverride());
    REQUIRE(registry.Fragment() == "Cell/lighting_cell.frag.spv");
    REQUIRE(registry.FragmentOrDefault() == "Cell/lighting_cell.frag.spv");

    registry.Clear();
    REQUIRE_FALSE(registry.HasOverride());
    REQUIRE(registry.FragmentOrDefault() ==
            fra::LightingTechniqueRegistry::kDefaultFragment);

    registry.SetFragment(std::string {});
    REQUIRE_FALSE(registry.HasOverride());
}
