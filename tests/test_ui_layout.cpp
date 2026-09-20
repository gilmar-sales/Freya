#include <Freya/Core/UiContext.hpp>
#include <Freya/Core/UiDraw.hpp>
#include <Freya/Core/UiStyle.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("UiStyle Default has expected tokens", "[ui]")
{
    auto s = fra::UiStyle::Default();
    REQUIRE(s.Var(fra::UiVar::FontSize) > 0.f);
    REQUIRE(s.Color(fra::UiCol::Text).a > 0.f);
}

TEST_CASE("UiContext Begin End and Button", "[ui]")
{
    fra::UiDraw    draw;
    fra::UiContext ui(&draw);
    ui.Begin(0.016f, { 1920, 1080 });
    REQUIRE(ui.Scale() == Catch::Approx(1.f));
    // No click — button returns false
    REQUIRE_FALSE(ui.Button("Go", { 100, 40 }));
    ui.End();
    REQUIRE_FALSE(draw.Empty());
}

TEST_CASE("UiContext style push pop", "[ui]")
{
    fra::UiDraw    draw;
    fra::UiContext ui(&draw);
    const auto     base = ui.Style().Color(fra::UiCol::Button);
    ui.PushStyleColor(fra::UiCol::Button, { 1, 0, 0, 1 });
    REQUIRE(ui.Style().Color(fra::UiCol::Button).r == Catch::Approx(1.f));
    ui.PopStyleColor();
    REQUIRE(ui.Style().Color(fra::UiCol::Button).r == Catch::Approx(base.r));
}
