#include <Freya/Asset/LightingTechniqueRegistry.hpp>

#include <gtest/gtest.h>

#include <string>

TEST(LightingTechniqueRegistry, DefaultsAndOverride)
{
    fra::LightingTechniqueRegistry registry;

    EXPECT_FALSE(registry.HasOverride());
    EXPECT_TRUE(registry.Fragment().empty());
    EXPECT_EQ(registry.FragmentOrDefault(),
              fra::LightingTechniqueRegistry::kDefaultFragment);

    registry.SetFragment("Cell/lighting_cell.frag.spv");
    EXPECT_TRUE(registry.HasOverride());
    EXPECT_EQ(registry.Fragment(), "Cell/lighting_cell.frag.spv");
    EXPECT_EQ(registry.FragmentOrDefault(), "Cell/lighting_cell.frag.spv");

    registry.Clear();
    EXPECT_FALSE(registry.HasOverride());
    EXPECT_EQ(registry.FragmentOrDefault(),
              fra::LightingTechniqueRegistry::kDefaultFragment);

    registry.SetFragment(std::string {});
    EXPECT_FALSE(registry.HasOverride());
}
