#include <Freya/Asset/Material.hpp>
#include <Freya/Containers/SparseSet.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("SparseSet insert contains remove", "[containers]")
{
    fra::SparseSet<fra::Material> set { 32 };

    fra::Material a { .id = 3 };
    fra::Material b { .id = 7 };

    REQUIRE_FALSE(set.contains(3));
    set.insert(a);
    set.insert(b);
    REQUIRE(set.contains(3));
    REQUIRE(set.contains(7));
    REQUIRE(set.size() == 2);

    set.insert(a);
    REQUIRE(set.size() == 2);

    set.remove(a);
    REQUIRE_FALSE(set.contains(3));
    REQUIRE(set.contains(7));
    REQUIRE(set.size() == 1);

    set.remove(b);
    REQUIRE_FALSE(set.contains(7));
    REQUIRE(set.size() == 0);
}
