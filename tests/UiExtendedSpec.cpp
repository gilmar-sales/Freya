#include <Freya/Core/UiContext.hpp>
#include <Freya/Core/UiDraw.hpp>
#include <Freya/Events/EventManager.hpp>
#include <Freya/Events/Keyboard.hpp>
#include <Freya/Events/Mouse.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
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

TEST(UiExtended, WindowDragMovesRect)
{
    fra::UiDraw       draw;
    fra::UiContext    ui(&draw);
    fra::EventManager events;
    ui.BindEvents(events);

    fra::UiWindowOpts opts {};
    opts.defaultPos = { 100.f, 100.f };

    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_TRUE(ui.BeginWindow("w1", "Tools", { 400.f, 300.f }, opts));
    ui.EndWindow();
    ui.End();
    const auto r0 = ui.WindowRect("w1");
    EXPECT_NEAR(r0.x, 100.f, 1e-3f);

    // Grab title bar center and drag +60/+40.
    MoveMouse(events, r0.x + 200.f, r0.y + 15.f);
    events.Send(
        fra::MouseButtonPressedEvent { .button = fra::MouseButton::Left });
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_TRUE(ui.BeginWindow("w1", "Tools", { 400.f, 300.f }, opts));
    ui.EndWindow();
    ui.End();

    MoveMouse(events, r0.x + 260.f, r0.y + 55.f);
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_TRUE(ui.BeginWindow("w1", "Tools", { 400.f, 300.f }, opts));
    ui.EndWindow();
    ui.End();

    events.Send(
        fra::MouseButtonReleasedEvent { .button = fra::MouseButton::Left });
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_TRUE(ui.BeginWindow("w1", "Tools", { 400.f, 300.f }, opts));
    ui.EndWindow();
    ui.End();

    const auto r1 = ui.WindowRect("w1");
    EXPECT_NEAR(r1.x, r0.x + 60.f, 1.f);
    EXPECT_NEAR(r1.y, r0.y + 40.f, 1.f);
    ui.UnbindEvents(events);
}

TEST(UiExtended, WindowResizeAndClose)
{
    fra::UiDraw       draw;
    fra::UiContext    ui(&draw);
    fra::EventManager events;
    ui.BindEvents(events);

    fra::UiWindowOpts opts {};
    opts.closable = true;

    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_TRUE(ui.BeginWindow("w2", "Panel", { 400.f, 300.f }, opts));
    ui.EndWindow();
    ui.End();
    const auto r0 = ui.WindowRect("w2");

    // Drag resize grip +40/+30.
    MoveMouse(events, r0.x + r0.w - 9.f, r0.y + r0.h - 9.f);
    events.Send(
        fra::MouseButtonPressedEvent { .button = fra::MouseButton::Left });
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_TRUE(ui.BeginWindow("w2", "Panel", { 400.f, 300.f }, opts));
    ui.EndWindow();
    ui.End();

    MoveMouse(events, r0.x + r0.w - 9.f + 40.f, r0.y + r0.h - 9.f + 30.f);
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_TRUE(ui.BeginWindow("w2", "Panel", { 400.f, 300.f }, opts));
    ui.EndWindow();
    ui.End();
    events.Send(
        fra::MouseButtonReleasedEvent { .button = fra::MouseButton::Left });

    const auto r1 = ui.WindowRect("w2");
    EXPECT_NEAR(r1.w, r0.w + 40.f, 1.5f);
    EXPECT_NEAR(r1.h, r0.h + 30.f, 1.5f);

    // Close via X button (title bar right).
    MoveMouse(events, r1.x + r1.w - 15.f, r1.y + 15.f);
    ClickLeft(events);
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_FALSE(ui.BeginWindow("w2", "Panel", { 400.f, 300.f }, opts));
    ui.EndWindow();
    ui.End();
    EXPECT_FALSE(ui.IsWindowOpen("w2"));

    ui.SetWindowOpen("w2", true);
    EXPECT_TRUE(ui.IsWindowOpen("w2"));
    ui.UnbindEvents(events);
}

TEST(UiExtended, SwitcherWizardDrawerPaginate)
{
    fra::UiDraw       draw;
    fra::UiContext    ui(&draw);
    fra::EventManager events;
    ui.BindEvents(events);

    // Switcher + drawer anchoring.
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_TRUE(ui.BeginDrawer("dock", fra::UiAnchor::Left, 300.f));
    ui.Label("docked");
    EXPECT_NEAR(ui.LastItemRect().x, 12.f, 1.f);
    ui.EndDrawer();
    EXPECT_TRUE(ui.BeginSwitcher("sw", { 400.f, 200.f }));
    ui.Label("page0");
    ui.EndSwitcher();
    std::array<std::string_view, 3> steps { "One", "Two", "Three" };
    EXPECT_TRUE(ui.BeginWizard("wiz", steps, 0, { 500.f, 300.f }));
    ui.Label("step0");
    ui.EndWizard();
    ui.End();

    // Wizard Next at col1 (x=120..240); columns start below container.
    ui.Begin(0.016f, { 1920, 1080 });
    const int nav = ui.WizardNav("wiznav", 0, 3, { 500.f, 36.f });
    (void) nav;
    ui.End();
    MoveMouse(events, 180.f, 36.f + 8.f + 16.f);
    ClickLeft(events);
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_EQ(ui.WizardNav("wiznav", 0, 3, { 500.f, 36.f }), 1);
    ui.End();

    // Paginate: container at (0,0,220,32), next at x=180..220.
    int page = 0;
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_FALSE(ui.Paginate("pg", 95, 10, &page));
    ui.End();
    EXPECT_EQ(page, 0);
    int first = -1;
    int count = -1;
    fra::UiContext::PageRange(95, 10, 9, &first, &count);
    EXPECT_EQ(first, 90);
    EXPECT_EQ(count, 5);
    MoveMouse(events, 200.f, 16.f);
    ClickLeft(events);
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_TRUE(ui.Paginate("pg", 95, 10, &page));
    ui.End();
    EXPECT_EQ(page, 1);

    // Collapse drawer via toggle (top outer edge).
    MoveMouse(events, 300.f - 32.f + 12.f, 8.f + 12.f);
    ClickLeft(events);
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_FALSE(ui.BeginDrawer("dock", fra::UiAnchor::Left, 300.f));
    ui.EndDrawer();
    ui.End();
    EXPECT_FALSE(ui.IsDrawerOpen("dock"));
    ui.OpenDrawer("dock", true);
    EXPECT_TRUE(ui.IsDrawerOpen("dock"));
    ui.UnbindEvents(events);
}

TEST(UiExtended, FileDialogSelectsFile)
{
    namespace fs       = std::filesystem;
    const fs::path tmp = fs::temp_directory_path() / "freya_fd_test_ui";
    fs::remove_all(tmp);
    fs::create_directories(tmp / "sub");
    {
        std::ofstream(tmp / "a.txt") << "a";
        std::ofstream(tmp / "b.txt") << "b";
        std::ofstream(tmp / "c.bin") << "c";
    }

    fra::UiDraw       draw;
    fra::UiContext    ui(&draw);
    fra::EventManager events;
    ui.BindEvents(events);

    fra::UiFileDialogOpts opts {};
    opts.extensions = { ".txt" };
    ui.OpenFileDialog("fd", tmp.string());

    std::string out;
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_FALSE(ui.FileDialog("fd", "Open", opts, &out, { 560.f, 420.f }));
    ui.End();
    EXPECT_TRUE(out.empty());

    // Rows: "..", "sub", "a.txt" — click a.txt (row 2).
    // Modal at ((1920-560)/2,(1080-420)/2)=(680,330), pad 12.
    // Title 28 + path 16 -> list view y = 342+36+24 = 402, rows 28+8.
    MoveMouse(events, 960.f, 402.f + 2 * (28.f + 8.f) + 14.f);
    ClickLeft(events);
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_FALSE(ui.FileDialog("fd", "Open", opts, &out, { 560.f, 420.f }));
    ui.End();
    EXPECT_TRUE(out.empty());

    // Select button at col1 (x=812..932, y=630..662).
    MoveMouse(events, 872.f, 646.f);
    ClickLeft(events);
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_TRUE(ui.FileDialog("fd", "Open", opts, &out, { 560.f, 420.f }));
    ui.End();
    EXPECT_EQ(fs::path(out).filename().string(), "a.txt");

    // Missing directory degrades gracefully.
    ui.OpenFileDialog("fd2", (tmp / "nope").string());
    std::string out2;
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_FALSE(ui.FileDialog("fd2", "Open", opts, &out2, { 560.f, 420.f }));
    ui.End();
    EXPECT_TRUE(out2.empty());

    ui.UnbindEvents(events);
    fs::remove_all(tmp);
}
