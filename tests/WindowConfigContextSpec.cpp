#include "Freya/Core/WindowConfigContext.hpp"
#include <Freya/Builders/FreyaOptionsBuilder.hpp>
#include <Freya/FreyaOptions.hpp>

#include <gtest/gtest.h>

TEST(WindowConfigContext, FreyaOptionsCopySeedsIndependentContext)
{
    fra::FreyaOptionsTemplate defaults;
    defaults.options             = skr::MakeArc<fra::FreyaOptions>();
    defaults.options->title      = "Main";
    defaults.options->width      = 1920;
    defaults.options->height     = 1080;
    defaults.options->vSync      = true;
    defaults.options->frameCount = 3;

    fra::WindowConfigContext mainCtx;
    mainCtx.options = defaults.options;

    EXPECT_EQ(mainCtx.options->title, "Main");
    EXPECT_EQ(mainCtx.options.get(), defaults.options.get());

    fra::FreyaOptionsBuilder secondaryBuilder;
    *secondaryBuilder.Build() = *defaults.options;
    secondaryBuilder.SetTitle("Game View")
        .SetWidth(1280)
        .SetHeight(720)
        .SetVSync(false);

    fra::WindowConfigContext secondaryCtx;
    secondaryCtx.options = secondaryBuilder.Build();

    EXPECT_NE(secondaryCtx.options.get(), defaults.options.get());
    EXPECT_EQ(secondaryCtx.options->title, "Game View");
    EXPECT_EQ(secondaryCtx.options->width, 1280u);
    EXPECT_EQ(secondaryCtx.options->height, 720u);
    EXPECT_FALSE(secondaryCtx.options->vSync);

    // Template / main options remain unchanged by the secondary clone.
    EXPECT_EQ(defaults.options->title, "Main");
    EXPECT_EQ(defaults.options->width, 1920u);
    EXPECT_TRUE(defaults.options->vSync);
    EXPECT_EQ(defaults.options->frameCount, 3u);

    // Shared template mutation is visible through the main context seed.
    defaults.options->frameCount = 4;
    EXPECT_EQ(mainCtx.options->frameCount, 4u);
    EXPECT_EQ(secondaryCtx.options->frameCount, 3u);
}

TEST(WindowConfigContext, StartsUnseeded)
{
    fra::WindowConfigContext ctx;
    EXPECT_EQ(ctx.options, nullptr);
}
