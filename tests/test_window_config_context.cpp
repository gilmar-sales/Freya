#include "Freya/Core/WindowConfigContext.hpp"
#include <Freya/Builders/FreyaOptionsBuilder.hpp>
#include <Freya/FreyaOptions.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("FreyaOptions copy seeds an independent WindowConfigContext",
          "[multi-window]")
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

    REQUIRE(mainCtx.options->title == "Main");
    REQUIRE(mainCtx.options.get() == defaults.options.get());

    fra::FreyaOptionsBuilder secondaryBuilder;
    *secondaryBuilder.Build() = *defaults.options;
    secondaryBuilder.SetTitle("Game View")
        .SetWidth(1280)
        .SetHeight(720)
        .SetVSync(false);

    fra::WindowConfigContext secondaryCtx;
    secondaryCtx.options = secondaryBuilder.Build();

    REQUIRE(secondaryCtx.options.get() != defaults.options.get());
    REQUIRE(secondaryCtx.options->title == "Game View");
    REQUIRE(secondaryCtx.options->width == 1280);
    REQUIRE(secondaryCtx.options->height == 720);
    REQUIRE_FALSE(secondaryCtx.options->vSync);

    // Template / main options remain unchanged by the secondary clone.
    REQUIRE(defaults.options->title == "Main");
    REQUIRE(defaults.options->width == 1920);
    REQUIRE(defaults.options->vSync);
    REQUIRE(defaults.options->frameCount == 3);

    // Shared template mutation is visible through the main context seed.
    defaults.options->frameCount = 4;
    REQUIRE(mainCtx.options->frameCount == 4);
    REQUIRE(secondaryCtx.options->frameCount == 3);
}

TEST_CASE("WindowConfigContext starts unseeded", "[multi-window]")
{
    fra::WindowConfigContext ctx;
    REQUIRE(ctx.options == nullptr);
}
