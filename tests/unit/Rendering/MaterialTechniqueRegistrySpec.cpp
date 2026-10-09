#include <Freya/Asset/MaterialTechniqueRegistry.hpp>

#include <gtest/gtest.h>

#include <limits>

TEST(MaterialTechniqueRegistry, DefaultTechniqueIsImplicit)
{
    const fra::MaterialTechniqueRegistry registry;
    EXPECT_EQ(registry.Count(), 1u);
    EXPECT_TRUE(registry.Entries().empty());
    EXPECT_EQ(registry.Get(0), nullptr);
    EXPECT_EQ(registry.Get(1), nullptr);
}

TEST(MaterialTechniqueRegistry, RegistrationPreservesNameAndRelativePath)
{
    fra::MaterialTechniqueRegistry registry;
    const auto id = registry.Register("custom", "Material/custom.frag.spv");
    EXPECT_EQ(id, 1u);
    EXPECT_EQ(registry.Count(), 2u);
    const auto* entry = registry.Get(id);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->name, "custom");
    EXPECT_EQ(entry->fragmentRelative, "Material/custom.frag.spv");
    ASSERT_EQ(registry.Entries().size(), 1u);
    EXPECT_EQ(registry.Entries()[0].name, entry->name);
}

TEST(MaterialTechniqueRegistry, EmptyPathAndOverflowDoNotMutateRegistry)
{
    fra::MaterialTechniqueRegistry registry;
    EXPECT_EQ(registry.Register("invalid", ""), 0u);
    EXPECT_EQ(registry.Count(), 1u);
    for (std::uint32_t i = 1; i < registry.kMaxTechniques; ++i)
        EXPECT_EQ(registry.Register(std::to_string(i), "custom.frag.spv"), i);
    EXPECT_EQ(registry.Count(), registry.kMaxTechniques);
    EXPECT_EQ(registry.Register("overflow", "other.frag.spv"), 0u);
    EXPECT_EQ(registry.Count(), registry.kMaxTechniques);
    EXPECT_EQ(registry.Get(registry.kMaxTechniques), nullptr);
    EXPECT_EQ(registry.Get(std::numeric_limits<std::uint32_t>::max()), nullptr);
    ASSERT_NE(registry.Get(1), nullptr);
    EXPECT_EQ(registry.Get(1)->name, "1");
}
