#include <Freya/Core/Limits.hpp>
#include <Freya/Core/PostProcess.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

namespace
{
    bool MaskIncludes(const fra::PostProcessMaterialMask& mask,
                      const std::uint32_t                 matId)
    {
        if (mask.count == 0)
            return true;
        const auto word = mask.bits[matId >> 7u][(matId >> 5u) & 3u];
        return (word & (1u << (matId & 31u))) != 0u;
    }

    void Bind(fra::PostProcessMaterialMask& mask, const std::uint32_t matId)
    {
        if (matId >= fra::kMaxMaterialSets)
            return;
        const auto word = matId >> 5u;
        const auto bit  = 1u << (matId & 31u);
        auto&      lane = mask.bits[word >> 2u][word & 3u];
        if ((lane & bit) != 0)
            return;
        lane |= bit;
        ++mask.count;
    }
} // namespace

TEST_CASE("PostProcessMaterialMask std140 size covers 1024 ids", "[mask]")
{
    STATIC_REQUIRE(sizeof(fra::PostProcessMaterialMask) == 144);
    STATIC_REQUIRE(fra::kMaxMaterialSets == 32u * 8u * 4u);
}

TEST_CASE("material mask bits match cell.frag addressing", "[mask]")
{
    fra::PostProcessMaterialMask mask {};
    Bind(mask, 0);
    Bind(mask, 31);
    Bind(mask, 32);
    Bind(mask, 128);
    Bind(mask, 1023);
    Bind(mask, 1024);

    REQUIRE(mask.count == 5);
    REQUIRE(MaskIncludes(mask, 0));
    REQUIRE(MaskIncludes(mask, 31));
    REQUIRE(MaskIncludes(mask, 32));
    REQUIRE(MaskIncludes(mask, 128));
    REQUIRE(MaskIncludes(mask, 1023));
    REQUIRE_FALSE(MaskIncludes(mask, 1));
    REQUIRE_FALSE(MaskIncludes(mask, 127));
}
