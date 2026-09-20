#include <Freya/Core/UiContext.hpp>
#include <Freya/Core/UiDraw.hpp>
#include <Freya/Core/UiStyle.hpp>
#include <Freya/Events/EventManager.hpp>
#include <Freya/Events/Keyboard.hpp>
#include <Freya/Events/Mouse.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace
{
    void MoveMouse(fra::EventManager& events, float x, float y)
    {
        events.Send(fra::MouseMoveEvent { .x      = x,
                                          .y      = y,
                                          .deltaX = 0.f,
                                          .deltaY = 0.f });
    }

    void ClickLeft(fra::EventManager& events)
    {
        events.Send(
            fra::MouseButtonPressedEvent { .button = fra::MouseButton::Left });
        events.Send(
            fra::MouseButtonReleasedEvent { .button = fra::MouseButton::Left });
    }

    void ClickRight(fra::EventManager& events)
    {
        events.Send(
            fra::MouseButtonPressedEvent { .button = fra::MouseButton::Right });
        events.Send(fra::MouseButtonReleasedEvent {
            .button = fra::MouseButton::Right });
    }
} // namespace

TEST_CASE("UiStyle Default has expected tokens", "[ui]")
{
    auto s = fra::UiStyle::Default();
    REQUIRE(s.Var(fra::UiVar::FontSize) > 0.f);
    REQUIRE(s.Color(fra::UiCol::Text).a > 0.f);
    REQUIRE(s.Var(fra::UiVar::ItemSpacing) > 0.f);
}

TEST_CASE("UiContext Begin End and Button without click", "[ui]")
{
    fra::UiDraw    draw;
    fra::UiContext ui(&draw);
    ui.Begin(0.016f, { 1920, 1080 });
    REQUIRE_THAT(ui.Scale(), WithinAbs(1.f, 1e-5f));
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
    REQUIRE_THAT(ui.Style().Color(fra::UiCol::Button).r, WithinAbs(1.f, 1e-5f));
    ui.PopStyleColor();
    REQUIRE_THAT(ui.Style().Color(fra::UiCol::Button).r,
                 WithinAbs(base.r, 1e-5f));
}

TEST_CASE("UiContext scales logical size to framebuffer", "[ui]")
{
    fra::UiDraw    draw;
    fra::UiContext ui(&draw);
    ui.SetReferenceSize({ 1920.f, 1080.f });
    ui.Begin(0.016f, { 960, 540 });
    REQUIRE_THAT(ui.Scale(), WithinAbs(0.5f, 1e-5f));
    REQUIRE_THAT(ui.LogicalSize().x, WithinAbs(1920.f, 1e-3f));
    REQUIRE_THAT(ui.LogicalSize().y, WithinAbs(1080.f, 1e-3f));
    ui.End();
}

TEST_CASE("UiContext Button click via EventManager", "[ui]")
{
    fra::UiDraw       draw;
    fra::UiContext    ui(&draw);
    fra::EventManager events;
    ui.BindEvents(events);

    // Frame 1: place button, no click yet.
    ui.Begin(0.016f, { 1920, 1080 });
    REQUIRE_FALSE(ui.Button("HitMe", { 120, 40 }));
    const auto r = ui.LastItemRect();
    ui.End();

    MoveMouse(events, r.x + r.w * 0.5f, r.y + r.h * 0.5f);
    events.Send(
        fra::MouseButtonPressedEvent { .button = fra::MouseButton::Left });

    ui.Begin(0.016f, { 1920, 1080 });
    REQUIRE_FALSE(ui.Button("HitMe", { 120, 40 })); // press arms active
    ui.End();

    events.Send(
        fra::MouseButtonReleasedEvent { .button = fra::MouseButton::Left });

    ui.Begin(0.016f, { 1920, 1080 });
    REQUIRE(ui.Button("HitMe", { 120, 40 }));
    REQUIRE(ui.WantCaptureMouse());
    ui.End();

    ui.UnbindEvents(events);
}

TEST_CASE("UiContext tooltip and popup do not steal grid cells", "[ui]")
{
    fra::UiDraw       draw;
    fra::UiContext    ui(&draw);
    fra::EventManager events;
    ui.BindEvents(events);

    constexpr float kCell = 64.f;
    constexpr float kGap  = 4.f;

    // Open inventory modal and seed hover on first slot.
    ui.Begin(0.016f, { 1920, 1080 });
    REQUIRE(ui.BeginModal("inv", { 400, 300 }));
    ui.BeginGrid("g", 4, { kCell, kCell }, kGap);
    ui.ItemSlot("s0", {}, 0, false, { kCell, kCell });
    const auto slot0 = ui.LastItemRect();
    ui.ItemSlot("s1", {}, 0, false, { kCell, kCell });
    const auto slot1a = ui.LastItemRect();
    ui.EndGrid();
    ui.EndModal();
    ui.End();

    MoveMouse(events, slot0.Center().x, slot0.Center().y);

    // Hold hover long enough for tooltip; ensure slot1 stays put.
    for (int frame = 0; frame < 30; ++frame)
    {
        ui.Begin(0.016f, { 1920, 1080 });
        REQUIRE(ui.BeginModal("inv", { 400, 300 }));
        ui.BeginGrid("g", 4, { kCell, kCell }, kGap);
        ui.ItemSlot("s0", {}, 0, false, { kCell, kCell });
        if (ui.IsItemHovered() && ui.BeginTooltip("tip", 0.f))
        {
            ui.Label("Item");
            ui.Label("x3");
            ui.EndTooltip();
        }
        ui.ItemSlot("s1", {}, 0, false, { kCell, kCell });
        const auto slot1 = ui.LastItemRect();
        REQUIRE_THAT(slot1.x, WithinAbs(slot1a.x, 0.5f));
        REQUIRE_THAT(slot1.y, WithinAbs(slot1a.y, 0.5f));
        ui.EndGrid();
        ui.EndModal();
        ui.End();
    }

    // Right-click opens popup; later slots must keep grid placement.
    ClickRight(events);
    ui.Begin(0.016f, { 1920, 1080 });
    REQUIRE(ui.BeginModal("inv", { 400, 300 }));
    ui.BeginGrid("g", 4, { kCell, kCell }, kGap);
    ui.ItemSlot("s0", {}, 0, false, { kCell, kCell });
    if (ui.BeginPopupContextItem("ctx0"))
    {
        ui.Button("Use", { 100, 28 });
        ui.Button("Discard", { 100, 28 });
        ui.EndPopup();
    }
    ui.ItemSlot("s1", {}, 0, false, { kCell, kCell });
    const auto slot1b = ui.LastItemRect();
    REQUIRE_THAT(slot1b.x, WithinAbs(slot1a.x, 0.5f));
    REQUIRE_THAT(slot1b.y, WithinAbs(slot1a.y, 0.5f));
    ui.EndGrid();
    ui.EndModal();
    ui.End();

    // Overlay quads exist (popup drawn on top layer).
    std::vector<fra::UiQuad> snap;
    draw.Snapshot(snap);
    REQUIRE(snap.size() > 4);

    ui.UnbindEvents(events);
}

TEST_CASE("UiContext TextInput focus submit and WantTextInput", "[ui]")
{
    fra::UiDraw       draw;
    fra::UiContext    ui(&draw);
    fra::EventManager events;
    ui.BindEvents(events);

    std::string buffer;

    ui.Begin(0.016f, { 1920, 1080 });
    REQUIRE_FALSE(ui.TextInput("chat", buffer, 64, { 200, 32 }));
    const auto r = ui.LastItemRect();
    ui.End();
    REQUIRE_FALSE(ui.WantTextInput());

    MoveMouse(events, r.Center().x, r.Center().y);
    events.Send(
        fra::MouseButtonPressedEvent { .button = fra::MouseButton::Left });

    ui.Begin(0.016f, { 1920, 1080 });
    REQUIRE_FALSE(ui.TextInput("chat", buffer, 64, { 200, 32 }));
    REQUIRE(ui.WantTextInput());
    ui.End();

    events.Send(fra::TextInputEvent { .text = "hi" });
    events.Send(fra::KeyPressedEvent { .key = fra::KeyCode::Return });

    ui.Begin(0.016f, { 1920, 1080 });
    REQUIRE(ui.TextInput("chat", buffer, 64, { 200, 32 }));
    REQUIRE(buffer == "hi");
    ui.End();

    ui.UnbindEvents(events);
}

TEST_CASE("UiContext FocusTextInput primes IME without click", "[ui]")
{
    fra::UiDraw    draw;
    fra::UiContext ui(&draw);
    std::string    buffer = "pre";

    ui.Begin(0.016f, { 1920, 1080 });
    ui.FocusTextInput("chat", buffer);
    REQUIRE_FALSE(ui.TextInput("chat", buffer, 64, { 200, 32 }));
    REQUIRE(ui.WantTextInput());
    REQUIRE(ui.MouseCursor() == fra::UiMouseCursor::Arrow);
    ui.End();
    REQUIRE(ui.WantTextInput());
}

TEST_CASE("UiContext checkbox and slider mutate values", "[ui]")
{
    fra::UiDraw       draw;
    fra::UiContext    ui(&draw);
    fra::EventManager events;
    ui.BindEvents(events);

    bool  flag  = false;
    float value = 0.25f;

    ui.Begin(0.016f, { 1920, 1080 });
    ui.Checkbox("Toggle", &flag);
    const auto checkR = ui.LastItemRect();
    ui.SliderFloat("Amt", &value, 0.f, 1.f, { 200, 24 });
    const auto sliderR = ui.LastItemRect();
    ui.End();

    MoveMouse(events, checkR.x + 11.f, checkR.y + 11.f);
    ClickLeft(events);

    ui.Begin(0.016f, { 1920, 1080 });
    REQUIRE(ui.Checkbox("Toggle", &flag));
    REQUIRE(flag);
    ui.End();

    // Drag slider grab toward the right edge.
    MoveMouse(events, sliderR.x + sliderR.w * 0.9f, sliderR.Center().y);
    events.Send(
        fra::MouseButtonPressedEvent { .button = fra::MouseButton::Left });
    ui.Begin(0.016f, { 1920, 1080 });
    ui.Checkbox("Toggle", &flag);
    ui.SliderFloat("Amt", &value, 0.f, 1.f, { 200, 24 });
    ui.End();
    events.Send(
        fra::MouseButtonReleasedEvent { .button = fra::MouseButton::Left });

    REQUIRE(value > 0.5f);
    ui.UnbindEvents(events);
}

TEST_CASE("UiContext workers can fill UiDraw while context draws",
          "[ui][thread]")
{
    // Contract: UiDraw is the thread-safe queue; UiContext is main-thread.
    // Workers may push Rect/Image concurrently with main-thread widgets.
    fra::UiDraw    draw(4096);
    fra::UiContext ui(&draw);

    std::atomic<bool> stop { false };
    std::thread       worker([&] {
        int i = 0;
        while (!stop.load(std::memory_order_relaxed))
        {
            draw.Rect({ static_cast<float>(i++ % 50), 200.f, 4.f, 4.f },
                      { 0.4f, 0.6f, 0.9f, 1.f });
        }
    });

    for (int frame = 0; frame < 40; ++frame)
    {
        ui.Begin(0.016f, { 1920, 1080 });
        ui.BeginAnchor(fra::UiAnchor::TopLeft, { 8.f, 8.f });
        ui.Label("HUD");
        ui.ProgressBar(0.5f, { 120.f, 12.f });
        ui.EndAnchor();
        if (ui.BeginModal("m", { 240, 160 }))
        {
            ui.Button("OK", { 80, 28 });
            ui.EndModal();
        }
        ui.End();

        std::vector<fra::UiQuad> snap;
        draw.Snapshot(snap);
        REQUIRE_FALSE(snap.empty());
        REQUIRE(snap.size() <= draw.MaxQuads());
        draw.Clear();
    }

    stop.store(true, std::memory_order_relaxed);
    worker.join();
}
