#include <Freya/Scene/AssetHandle.hpp>

#include <gtest/gtest.h>

#include <type_traits>
#include <unordered_set>

TEST(AssetHandle, NullDiffersFromEngagedZero)
{
    constexpr fra::MeshHandle null;
    constexpr fra::MeshHandle zero(0);
    static_assert(!null.IsValid());
    static_assert(zero.IsValid());
    static_assert(null != zero);
    EXPECT_FALSE(static_cast<bool>(null));
    EXPECT_TRUE(static_cast<bool>(zero));
    EXPECT_EQ(zero.Id(), 0u);
}

TEST(AssetHandle, EqualityOrderingAndHashRespectValidityAndId)
{
    const fra::MeshHandle null;
    const fra::MeshHandle zero(0);
    const fra::MeshHandle one(1);
    EXPECT_EQ(null, fra::MeshHandle {});
    EXPECT_EQ(one, fra::MeshHandle(1));
    EXPECT_LT(null, zero);
    EXPECT_LT(zero, one);
    std::unordered_set<fra::MeshHandle> handles { null, zero, one,
                                                  fra::MeshHandle(1) };
    EXPECT_EQ(handles.size(), 3u);
    EXPECT_TRUE(handles.contains(null));
    EXPECT_TRUE(handles.contains(zero));
    EXPECT_EQ(std::hash<fra::MeshHandle> {}(one),
              std::hash<fra::MeshHandle> {}(fra::MeshHandle(1)));
}

TEST(AssetHandle, TagsPreventAccidentalCrossPoolConversions)
{
    static_assert(!std::is_convertible_v<fra::MeshHandle, fra::MaterialHandle>);
    static_assert(!std::is_convertible_v<fra::TextureHandle, fra::MeshHandle>);
    static_assert(!std::is_convertible_v<std::uint32_t, fra::MeshHandle>);
    SUCCEED();
}
