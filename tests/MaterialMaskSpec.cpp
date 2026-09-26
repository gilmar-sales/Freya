#include <Freya/Core/Limits.hpp>
#include <Freya/Core/PostProcess.hpp>

#include <gtest/gtest.h>

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

TEST(MaterialMask, Std140SizeCovers1024Ids)
{
    static_assert(sizeof(fra::PostProcessMaterialMask) == 144);
    static_assert(fra::kMaxMaterialSets == 32u * 8u * 4u);
}

TEST(MaterialMask, BitsMatchCellFragAddressing)
{
    fra::PostProcessMaterialMask mask {};
    Bind(mask, 0);
    Bind(mask, 31);
    Bind(mask, 32);
    Bind(mask, 128);
    Bind(mask, 1023);
    Bind(mask, 1024);

    EXPECT_EQ(mask.count, 5u);
    EXPECT_TRUE(MaskIncludes(mask, 0));
    EXPECT_TRUE(MaskIncludes(mask, 31));
    EXPECT_TRUE(MaskIncludes(mask, 32));
    EXPECT_TRUE(MaskIncludes(mask, 128));
    EXPECT_TRUE(MaskIncludes(mask, 1023));
    EXPECT_FALSE(MaskIncludes(mask, 1));
    EXPECT_FALSE(MaskIncludes(mask, 127));
}
