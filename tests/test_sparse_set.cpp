#include "Freya/Containers/SparseSet.hpp"
#include <Freya/Asset/Material.hpp>

#include <gtest/gtest.h>

TEST(SparseSet, InsertContainsRemove)
{
    fra::SparseSet<fra::Material> set { 32 };

    fra::Material a { .id = 3 };
    fra::Material b { .id = 7 };

    EXPECT_FALSE(set.contains(3));
    set.insert(a);
    set.insert(b);
    EXPECT_TRUE(set.contains(3));
    EXPECT_TRUE(set.contains(7));
    EXPECT_EQ(set.size(), 2u);

    set.insert(a);
    EXPECT_EQ(set.size(), 2u);

    set.remove(a);
    EXPECT_FALSE(set.contains(3));
    EXPECT_TRUE(set.contains(7));
    EXPECT_EQ(set.size(), 1u);

    set.remove(b);
    EXPECT_FALSE(set.contains(7));
    EXPECT_EQ(set.size(), 0u);
}
