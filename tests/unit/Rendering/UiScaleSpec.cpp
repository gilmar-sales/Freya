#include <Freya/Core/UiContext.hpp>

#include <gtest/gtest.h>

TEST(UiScale, ReferenceResolutionIsUnitScale)
{
    fra::UiContext ui;
    EXPECT_FLOAT_EQ(ui.ScaleForExtent({ 1920, 1080 }), 1.0f);
}

TEST(UiScale, FourKDoublesTheUi)
{
    fra::UiContext ui;
    EXPECT_FLOAT_EQ(ui.ScaleForExtent({ 3840, 2160 }), 2.0f);
}

TEST(UiScale, UsesTheSmallerAxis)
{
    fra::UiContext ui;
    // 3840x2036 (maximized 4K window): height is the limiting axis.
    EXPECT_NEAR(ui.ScaleForExtent({ 3840, 2036 }), 2036.0f / 1080.0f, 1e-5f);
    // Ultrawide: width is the limiting axis.
    EXPECT_NEAR(ui.ScaleForExtent({ 2560, 1440 }), 2560.0f / 1920.0f, 1e-5f);
}

TEST(UiScale, FollowsReferenceSize)
{
    fra::UiContext ui;
    ui.SetReferenceSize({ 1280.f, 720.f });
    EXPECT_FLOAT_EQ(ui.ScaleForExtent({ 2560, 1440 }), 2.0f);
}

TEST(UiScale, ZeroExtentFallsBackToUnit)
{
    fra::UiContext ui;
    EXPECT_FLOAT_EQ(ui.ScaleForExtent({ 0, 0 }), 1.0f);
}

TEST(UiScale, BeginMatchesScaleForExtent)
{
    fra::UiContext ui;
    EXPECT_FALSE(ui.HasBegun());
    ui.Begin(0.016f, { 3840, 2160 });
    EXPECT_TRUE(ui.HasBegun());
    EXPECT_FLOAT_EQ(ui.Scale(), ui.ScaleForExtent({ 3840, 2160 }));
    ui.End();
}
