#include <Freya/Core/UiContext.hpp>
#include <Freya/Core/UiDraw.hpp>
#include <Freya/Events/EventManager.hpp>
#include <Freya/Events/Keyboard.hpp>
#include <Freya/Events/Mouse.hpp>

#include <gtest/gtest.h>

#include <string>

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
} // namespace

TEST(UiExtended, RadioToggleSpinCombo)
{
    fra::UiDraw       draw;
    fra::UiContext    ui(&draw);
    fra::EventManager events;
    ui.BindEvents(events);

    bool                            flag = false;
    int                             num  = 5;
    int                             sel  = 0;
    std::array<std::string_view, 3> items { "A", "B", "C" };

    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_FALSE(ui.RadioButton("R1", false));
    EXPECT_FALSE(ui.ToggleSwitch("T1", &flag));
    EXPECT_FALSE(flag);
    ui.SliderInt("N", &num, 0, 10, { 200, 24 });
    ui.ComboBox("Cb", items, &sel, { 220, 32 });
    const auto r = ui.LastItemRect();
    ui.End();
    (void) r;

    // Toggle via click on switch rect.
    ui.Begin(0.016f, { 1920, 1080 });
    ui.ToggleSwitch("T1", &flag);
    const auto sw = ui.LastItemRect();
    ui.End();
    MoveMouse(events, sw.x + 10.f, sw.y + 10.f);
    ClickLeft(events);
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_TRUE(ui.ToggleSwitch("T1", &flag));
    ui.End();
    EXPECT_TRUE(flag);
    ui.UnbindEvents(events);
}

TEST(UiExtended, DoubleClickTracked)
{
    fra::UiDraw       draw;
    fra::UiContext    ui(&draw);
    fra::EventManager events;
    ui.BindEvents(events);

    ui.Begin(0.016f, { 1920, 1080 });
    ui.Button("Db", { 120, 40 });
    const auto r = ui.LastItemRect();
    ui.End();

    MoveMouse(events, r.Center().x, r.Center().y);
    ClickLeft(events);
    ui.Begin(0.016f, { 1920, 1080 });
    ui.Button("Db", { 120, 40 });
    ui.End();
    // Second click quickly: press+release then Begin -> clicked + double.
    events.Send(
        fra::MouseButtonPressedEvent { .button = fra::MouseButton::Left });
    events.Send(
        fra::MouseButtonReleasedEvent { .button = fra::MouseButton::Left });
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_TRUE(ui.Button("Db", { 120, 40 }));
    EXPECT_TRUE(ui.IsItemDoubleClicked());
    ui.End();
    ui.UnbindEvents(events);
}

TEST(UiExtended, ContainersAndHeaderToast)
{
    fra::UiDraw    draw;
    fra::UiContext ui(&draw);

    ui.Begin(0.016f, { 1920, 1080 });
    std::array<float, 2> w { 1.f, 2.f };
    EXPECT_TRUE(ui.BeginRow("row", w, 8.f, 40.f));
    ui.Button("A", { 10, 10 });
    ui.NextCell();
    ui.Button("B", { 10, 10 });
    ui.EndRow();
    EXPECT_TRUE(ui.BeginMargin("m", 8.f));
    ui.Label("inside");
    ui.EndMargin();
    EXPECT_TRUE(ui.BeginCenter("c", { 200, 100 }));
    ui.Label("centered");
    ui.EndCenter();
    EXPECT_TRUE(ui.BeginVStack("vs", 4.f));
    ui.Label("stacked");
    ui.EndVStack();
    ui.Heading("Title");
    ui.Bullet("point");
    ui.LabelColored("colored", { 1, 0, 0, 1 });
    const bool open = ui.CollapsingHeader("h", "Section", true);
    EXPECT_TRUE(open);
    ui.Spinner("sp", { 24, 24 });
    ui.ShowToast("hello", 2.f);
    ui.End();

    std::vector<fra::UiQuad> snap;
    draw.Snapshot(snap);
    EXPECT_FALSE(snap.empty());
}

TEST(UiExtended, DisabledBlocksInput)
{
    fra::UiDraw       draw;
    fra::UiContext    ui(&draw);
    fra::EventManager events;
    ui.BindEvents(events);

    ui.Begin(0.016f, { 1920, 1080 });
    ui.BeginDisabled(true);
    ui.Button("Nope", { 120, 40 });
    const auto r = ui.LastItemRect();
    ui.EndDisabled();
    ui.End();

    MoveMouse(events, r.Center().x, r.Center().y);
    ClickLeft(events);
    ui.Begin(0.016f, { 1920, 1080 });
    ui.BeginDisabled(true);
    EXPECT_FALSE(ui.Button("Nope", { 120, 40 }));
    ui.EndDisabled();
    ui.End();
    ui.UnbindEvents(events);
}

TEST(UiExtended, StyleIniRoundTrip)
{
    auto s                      = fra::UiStyle::Default();
    s.Color(fra::UiCol::Button) = { 1, 0, 0, 1 };
    s.Var(fra::UiVar::Rounding) = 9.f;
    const std::string ini       = s.SaveIni();
    EXPECT_FALSE(ini.empty());

    auto t = fra::UiStyle::Default();
    EXPECT_TRUE(t.LoadIni(ini));
    EXPECT_NEAR(t.Color(fra::UiCol::Button).r, 1.f, 1e-4f);
    EXPECT_NEAR(t.Var(fra::UiVar::Rounding), 9.f, 1e-4f);
    EXPECT_FALSE(t.LoadIni("BogusKey=1\n"));
}

TEST(UiExtended, TextCursorAndClipboard)
{
    fra::UiDraw       draw;
    fra::UiContext    ui(&draw);
    fra::EventManager events;
    ui.BindEvents(events);

    std::string buf = "abc";
    ui.FocusTextInput("ti", buf);
    ui.Begin(0.016f, { 1920, 1080 });
    ui.TextInput("ti", buf, 64, { 200, 32 });
    ui.End();
    EXPECT_EQ(buf, "abc");
    EXPECT_EQ(ui.MouseCursor(), fra::UiMouseCursor::IBeam);

    // Ctrl+C copies, Ctrl+X cuts, Ctrl+V pastes.
    events.Send(fra::KeyPressedEvent { .key = fra::KeyCode::LCtrl });
    events.Send(fra::KeyPressedEvent { .key = fra::KeyCode::C });
    events.Send(fra::KeyPressedEvent { .key = fra::KeyCode::X });
    EXPECT_TRUE(ui.Clipboard().size() > 0);
    events.Send(fra::KeyPressedEvent { .key = fra::KeyCode::V });
    events.Send(fra::KeyReleasedEvent { .key = fra::KeyCode::LCtrl });
    ui.Begin(0.016f, { 1920, 1080 });
    ui.TextInput("ti", buf, 64, { 200, 32 });
    ui.End();
    EXPECT_FALSE(buf.empty());
    ui.UnbindEvents(events);
}
