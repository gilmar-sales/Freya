#include <Freya/Asset/FontAtlas.hpp>
#include <Freya/Core/UiContext.hpp>
#include <Freya/Core/UiDraw.hpp>
#include <Freya/Events/EventManager.hpp>
#include <Freya/Events/Keyboard.hpp>
#include <Freya/Events/Mouse.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

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

    void PressLeft(fra::EventManager& events)
    {
        events.Send(
            fra::MouseButtonPressedEvent { .button = fra::MouseButton::Left });
    }

    void ReleaseLeft(fra::EventManager& events)
    {
        events.Send(
            fra::MouseButtonReleasedEvent { .button = fra::MouseButton::Left });
    }
} // namespace

TEST(UiBaseWidgets, SelectableClickHoverAndDisabled)
{
    fra::UiDraw       draw;
    fra::UiContext    ui(&draw);
    fra::EventManager events;
    ui.BindEvents(events);

    MoveMouse(events, 1500.f, 900.f);
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_FALSE(ui.Selectable("Opt", false, { 300.f, 30.f }));
    EXPECT_FALSE(ui.IsItemHovered());
    ui.End();

    MoveMouse(events, 150.f, 15.f);
    ClickLeft(events);
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_TRUE(ui.Selectable("Opt", false, { 300.f, 30.f }));
    EXPECT_TRUE(ui.IsItemHovered());
    EXPECT_TRUE(ui.IsItemClicked());
    ui.End();

    // Disabled selectables never fire, even under the cursor.
    MoveMouse(events, 150.f, 15.f);
    ClickLeft(events);
    ui.Begin(0.016f, { 1920, 1080 });
    ui.BeginDisabled(true);
    EXPECT_FALSE(ui.Selectable("Opt", false, { 300.f, 30.f }));
    ui.EndDisabled();
    ui.End();

    ui.UnbindEvents(events);
}

TEST(UiBaseWidgets, SliderIntDragMutates)
{
    fra::UiDraw       draw;
    fra::UiContext    ui(&draw);
    fra::EventManager events;
    ui.BindEvents(events);

    int value = 5;
    ui.Begin(0.016f, { 1920, 1080 });
    ui.SliderInt("N", &value, 0, 10, { 200.f, 24.f });
    const auto bar = ui.LastItemRect();
    ui.End();

    // Drag toward the right edge (press-and-hold, no release yet).
    MoveMouse(events, bar.x + bar.w * 0.95f, bar.Center().y);
    PressLeft(events);
    ui.Begin(0.016f, { 1920, 1080 });
    ui.SliderInt("N", &value, 0, 10, { 200.f, 24.f });
    ui.End();
    ReleaseLeft(events);
    EXPECT_GE(value, 9);

    // Drag toward the left edge.
    MoveMouse(events, bar.x + bar.w * 0.05f, bar.Center().y);
    PressLeft(events);
    ui.Begin(0.016f, { 1920, 1080 });
    ui.SliderInt("N", &value, 0, 10, { 200.f, 24.f });
    ui.End();
    ReleaseLeft(events);
    EXPECT_LE(value, 1);

    ui.UnbindEvents(events);
}

TEST(UiBaseWidgets, SpinBoxButtonsStep)
{
    fra::UiDraw       draw;
    fra::UiContext    ui(&draw);
    fra::EventManager events;
    ui.BindEvents(events);

    int value = 3;
    ui.Begin(0.016f, { 1920, 1080 });
    ui.SpinBox("party", &value, 0, 10, 1, { 320.f, 32.f });
    const auto plus = ui.LastItemRect();
    ui.End();

    MoveMouse(events, plus.Center().x, plus.Center().y);
    ClickLeft(events);
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_TRUE(ui.SpinBox("party", &value, 0, 10, 1, { 320.f, 32.f }));
    ui.End();
    EXPECT_EQ(value, 4);

    // "-" lives in column 0, i.e. x in [0,32] when first in frame.
    MoveMouse(events, 16.f, 16.f);
    ClickLeft(events);
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_TRUE(ui.SpinBox("party", &value, 0, 10, 1, { 320.f, 32.f }));
    ui.End();
    EXPECT_EQ(value, 3);

    ui.UnbindEvents(events);
}

TEST(UiBaseWidgets, ComboBoxSelectsItem)
{
    fra::UiDraw       draw;
    fra::UiContext    ui(&draw);
    fra::EventManager events;
    ui.BindEvents(events);

    std::array<std::string_view, 3> items { "Knight", "Ranger", "Scholar" };
    int                             sel = 0;

    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_FALSE(ui.ComboBox("cls", items, &sel, { 220.f, 32.f }));
    const auto box = ui.LastItemRect();
    ui.End();
    EXPECT_EQ(sel, 0);

    // Open the dropdown.
    MoveMouse(events, box.Center().x, box.Center().y);
    ClickLeft(events);
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_FALSE(ui.ComboBox("cls", items, &sel, { 220.f, 32.f }));
    ui.End();

    // Dropdown list starts at box bottom + 2 with 28px rows (+4px inner
    // padding): item 1 center is box.y + 32 + 2 + 4 + 28 + 14.
    MoveMouse(events, box.Center().x, box.y + 80.f);
    ClickLeft(events);
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_TRUE(ui.ComboBox("cls", items, &sel, { 220.f, 32.f }));
    ui.End();
    EXPECT_EQ(sel, 1);

    ui.UnbindEvents(events);
}

TEST(UiBaseWidgets, ColorEditDragRedChannel)
{
    fra::UiDraw       draw;
    fra::UiContext    ui(&draw);
    fra::EventManager events;
    ui.BindEvents(events);

    glm::vec3 rgb { 0.2f, 0.5f, 0.8f };
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_FALSE(ui.ColorEdit("tint", &rgb));
    ui.End();
    EXPECT_FLOAT_EQ(rgb.x, 0.2f);

    // First widget in frame: Label(16px) + 8px spacing, so the R bar is
    // at y in [24,46], full 200px wide.
    MoveMouse(events, 190.f, 35.f);
    PressLeft(events);
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_TRUE(ui.ColorEdit("tint", &rgb));
    ui.End();
    ReleaseLeft(events);
    EXPECT_GT(rgb.x, 0.9f);
    EXPECT_FLOAT_EQ(rgb.y, 0.5f);
    EXPECT_FLOAT_EQ(rgb.z, 0.8f);

    ui.UnbindEvents(events);
}

TEST(UiBaseWidgets, SearchBoxTypesAndClears)
{
    fra::UiDraw       draw;
    fra::UiContext    ui(&draw);
    fra::EventManager events;
    ui.BindEvents(events);

    std::string buf;
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_FALSE(ui.SearchBox("s", buf, 64, { 320.f, 32.f }));
    ui.End();

    // Focus the text field, then type.
    MoveMouse(events, 142.f, 16.f);
    ClickLeft(events);
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_FALSE(ui.SearchBox("s", buf, 64, { 320.f, 32.f }));
    ui.End();

    events.Send(fra::TextInputEvent { .text = "hi" });
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_FALSE(ui.SearchBox("s", buf, 64, { 320.f, 32.f }));
    ui.End();
    EXPECT_EQ(buf, "hi");

    // Clear button occupies the last 32px of the 320px row.
    MoveMouse(events, 300.f, 16.f);
    ClickLeft(events);
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_TRUE(ui.SearchBox("s", buf, 64, { 320.f, 32.f }));
    ui.End();
    EXPECT_TRUE(buf.empty());

    ui.UnbindEvents(events);
}

TEST(UiBaseWidgets, ItemSlotClickAndHover)
{
    fra::UiDraw       draw;
    fra::UiContext    ui(&draw);
    fra::EventManager events;
    ui.BindEvents(events);

    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_FALSE(ui.ItemSlot("s0", {}, 3, false, { 64.f, 64.f }));
    ui.End();

    const auto c = ui.LastItemRect().Center();
    MoveMouse(events, c.x, c.y);
    ClickLeft(events);
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_TRUE(ui.ItemSlot("s0", {}, 3, false, { 64.f, 64.f }));
    EXPECT_TRUE(ui.IsItemHovered());
    ui.End();

    std::vector<fra::UiQuad> snap;
    draw.Snapshot(snap);
    EXPECT_FALSE(snap.empty());

    ui.UnbindEvents(events);
}

TEST(UiBaseWidgets, AbilitySlotReadyVsCooldown)
{
    fra::UiDraw       draw;
    fra::UiContext    ui(&draw);
    fra::EventManager events;
    ui.BindEvents(events);

    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_FALSE(ui.AbilitySlot("ab", {}, "1", 0.f, { 64.f, 64.f }));
    ui.End();

    const auto c = ui.LastItemRect().Center();
    MoveMouse(events, c.x, c.y);
    ClickLeft(events);
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_TRUE(ui.AbilitySlot("ab", {}, "1", 0.f, { 64.f, 64.f }));
    ui.End();

    // Cooling down: clicks are swallowed while the radial is active.
    MoveMouse(events, c.x, c.y);
    ClickLeft(events);
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_FALSE(ui.AbilitySlot("ab", {}, "1", 1.f, { 64.f, 64.f }));
    ui.End();

    ui.UnbindEvents(events);
}

TEST(UiBaseWidgets, ListItemClickSelects)
{
    fra::UiDraw       draw;
    fra::UiContext    ui(&draw);
    fra::EventManager events;
    ui.BindEvents(events);

    int scroll = -1;
    ui.Begin(0.016f, { 1920, 1080 });
    ASSERT_TRUE(ui.BeginList("quests", { 300.f, 120.f }, 5, 24.f, &scroll));
    for (int i = 0; i < 5; ++i)
        EXPECT_FALSE(
            ui.ListItem(i, std::string("Q") + std::to_string(i), false));
    ui.EndList();
    ui.End();
    EXPECT_EQ(scroll, 0);

    // Items advance by height + spacing: item 2 spans y in [64,88].
    MoveMouse(events, 150.f, 76.f);
    ClickLeft(events);
    int sel = -1;
    ui.Begin(0.016f, { 1920, 1080 });
    ASSERT_TRUE(ui.BeginList("quests", { 300.f, 120.f }, 5, 24.f, nullptr));
    for (int i = 0; i < 5; ++i)
    {
        if (ui.ListItem(i, std::string("Q") + std::to_string(i), false))
            sel = i;
    }
    ui.EndList();
    ui.End();
    EXPECT_EQ(sel, 2);

    ui.UnbindEvents(events);
}

TEST(UiBaseWidgets, SetScrollHereYJumpsToBottom)
{
    fra::UiDraw    draw;
    fra::UiContext ui(&draw);

    ui.Begin(0.016f, { 1920, 1080 });
    ASSERT_TRUE(ui.BeginList("long", { 200.f, 100.f }, 100, 20.f, nullptr));
    for (int i = 0; i < 100; ++i)
        ui.ListItem(i, std::string("E") + std::to_string(i), false);
    ui.SetScrollHereY(1.f);
    ui.EndList();
    ui.End();

    // (2000 - 100) / 20 = first visible index 95 on the next frame.
    int scroll = -1;
    ui.Begin(0.016f, { 1920, 1080 });
    ASSERT_TRUE(ui.BeginList("long", { 200.f, 100.f }, 100, 20.f, &scroll));
    ui.EndList();
    ui.End();
    EXPECT_EQ(scroll, 95);
}

TEST(UiBaseWidgets, ScrollViewWheelScrollsContent)
{
    fra::UiDraw       draw;
    fra::UiContext    ui(&draw);
    fra::EventManager events;
    ui.BindEvents(events);

    MoveMouse(events, 100.f, 50.f);
    events.Send(fra::MouseWheelEvent { .x = 0.f, .y = -2.f });
    ui.Begin(0.016f, { 1920, 1080 });
    ASSERT_TRUE(ui.BeginScrollView("sv", { 200.f, 100.f }, 1000.f));
    ui.Label("content");
    EXPECT_NEAR(ui.LastItemRect().y, -80.f, 1e-3f);
    ui.EndScrollView();
    ui.End();

    ui.UnbindEvents(events);
}

TEST(UiBaseWidgets, TabBarSwitchesPages)
{
    fra::UiDraw       draw;
    fra::UiContext    ui(&draw);
    fra::EventManager events;
    ui.BindEvents(events);

    ui.Begin(0.016f, { 1920, 1080 });
    ASSERT_TRUE(ui.BeginTabBar("tabs"));
    EXPECT_TRUE(ui.Tab("A"));
    EXPECT_FALSE(ui.Tab("B"));
    ui.EndTabBar();
    ui.End();

    // Tab buttons are 100x32 stacked vertically: B center is (50, 56).
    // On the click frame A still reports selected (B runs after A);
    // the switch is visible from the next frame on.
    MoveMouse(events, 50.f, 56.f);
    ClickLeft(events);
    ui.Begin(0.016f, { 1920, 1080 });
    ASSERT_TRUE(ui.BeginTabBar("tabs"));
    EXPECT_TRUE(ui.Tab("A"));
    EXPECT_TRUE(ui.Tab("B"));
    ui.EndTabBar();
    ui.End();

    ui.Begin(0.016f, { 1920, 1080 });
    ASSERT_TRUE(ui.BeginTabBar("tabs"));
    EXPECT_FALSE(ui.Tab("A"));
    EXPECT_TRUE(ui.Tab("B"));
    ui.EndTabBar();
    ui.End();

    ui.UnbindEvents(events);
}

TEST(UiBaseWidgets, ColumnsGridAndModalGeometry)
{
    fra::UiDraw    draw;
    fra::UiContext ui(&draw);

    ui.Begin(0.016f, { 1920, 1080 });
    float widths[] = { 100.f, 200.f };
    ASSERT_TRUE(ui.BeginColumns("cols", 2, widths));
    EXPECT_FALSE(ui.Button("c0", { 50.f, 20.f }));
    EXPECT_NEAR(ui.LastItemRect().x, 0.f, 1e-3f);
    ui.NextColumn();
    EXPECT_FALSE(ui.Button("c1", { 50.f, 20.f }));
    EXPECT_NEAR(ui.LastItemRect().x, 100.f, 1e-3f);
    ui.EndColumns();
    ui.End();

    ui.Begin(0.016f, { 1920, 1080 });
    ASSERT_TRUE(ui.BeginGrid("g", 2, { 64.f, 64.f }, 8.f));
    ui.ItemSlot("g0", {}, 0, false, { 64.f, 64.f });
    ui.ItemSlot("g1", {}, 0, false, { 64.f, 64.f });
    ui.ItemSlot("g2", {}, 0, false, { 64.f, 64.f });
    EXPECT_NEAR(ui.LastItemRect().x, 0.f, 1e-3f);
    EXPECT_NEAR(ui.LastItemRect().y, 72.f, 1e-3f);
    ui.EndGrid();
    ui.End();

    // 400x300 modal is centered at (760,390); panel padding is 12.
    ui.Begin(0.016f, { 1920, 1080 });
    ASSERT_TRUE(ui.BeginModal("m", { 400.f, 300.f }));
    EXPECT_FALSE(ui.Button("ok", { 80.f, 28.f }));
    EXPECT_NEAR(ui.LastItemRect().x, 772.f, 1e-3f);
    EXPECT_NEAR(ui.LastItemRect().y, 402.f, 1e-3f);
    ui.EndModal();
    ui.End();
    EXPECT_TRUE(ui.WantCaptureMouse());
    EXPECT_TRUE(ui.WantCaptureKeyboard());
    EXPECT_TRUE(ui.WantCaptureGamepad());
}

TEST(UiBaseWidgets, CollapsingHeaderToggles)
{
    fra::UiDraw       draw;
    fra::UiContext    ui(&draw);
    fra::EventManager events;
    ui.BindEvents(events);

    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_TRUE(ui.CollapsingHeader("h", "Section", true));
    const auto r = ui.LastItemRect();
    ui.End();

    MoveMouse(events, r.Center().x, r.Center().y);
    ClickLeft(events);
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_FALSE(ui.CollapsingHeader("h", "Section", true));
    ui.End();

    ClickLeft(events);
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_TRUE(ui.CollapsingHeader("h", "Section", true));
    ui.End();

    ui.UnbindEvents(events);
}

TEST(UiBaseWidgets, TooltipDelayRespectsHoverTime)
{
    fra::UiDraw       draw;
    fra::UiContext    ui(&draw);
    fra::EventManager events;
    ui.BindEvents(events);

    MoveMouse(events, 32.f, 32.f);
    ui.Begin(0.016f, { 1920, 1080 });
    ui.ItemSlot("tip_slot", {}, 0, false, { 64.f, 64.f });
    ASSERT_TRUE(ui.IsItemHovered());
    EXPECT_FALSE(ui.BeginTooltip("tip"));
    ui.End();

    bool shown = false;
    for (int frame = 0; frame < 30; ++frame)
    {
        ui.Begin(0.016f, { 1920, 1080 });
        ui.ItemSlot("tip_slot", {}, 0, false, { 64.f, 64.f });
        if (ui.IsItemHovered() && ui.BeginTooltip("tip"))
        {
            ui.Label("Item");
            ui.EndTooltip();
            shown = true;
        }
        ui.End();
    }
    EXPECT_TRUE(shown);

    MoveMouse(events, 1500.f, 900.f);
    ui.Begin(0.016f, { 1920, 1080 });
    ui.ItemSlot("tip_slot", {}, 0, false, { 64.f, 64.f });
    EXPECT_FALSE(ui.BeginTooltip("tip"));
    ui.End();

    ui.UnbindEvents(events);
}

TEST(UiBaseWidgets, DragDropTransfersPayload)
{
    fra::UiDraw       draw;
    fra::UiContext    ui(&draw);
    fra::EventManager events;
    ui.BindEvents(events);

    auto drawSlots = [&] {
        ui.ItemSlot("da", {}, 0, false, { 64.f, 64.f });
        const auto a = ui.LastItemRect();
        ui.ItemSlot("db", {}, 0, false, { 64.f, 64.f });
        const auto b = ui.LastItemRect();
        return std::pair { a, b };
    };

    // Frame 1: geometry (da at y=0, db below it).
    ui.Begin(0.016f, { 1920, 1080 });
    const auto [ra, rb] = drawSlots();
    ui.End();

    // Frame 2: press on da (arms active, not dragging yet).
    MoveMouse(events, ra.Center().x, ra.Center().y);
    PressLeft(events);
    ui.Begin(0.016f, { 1920, 1080 });
    drawSlots();
    EXPECT_FALSE(ui.BeginDragDropSource("dd", 7));
    ui.End();

    // Frame 3: move (still held) onto db -> drag starts on da.
    MoveMouse(events, rb.Center().x, rb.Center().y);
    std::uint64_t got = 0;
    ui.Begin(0.016f, { 1920, 1080 });
    ui.ItemSlot("da", {}, 0, false, { 64.f, 64.f });
    EXPECT_TRUE(ui.BeginDragDropSource("dd", 7));
    EXPECT_TRUE(ui.IsDragDropActive());
    ui.ItemSlot("db", {}, 0, false, { 64.f, 64.f });
    EXPECT_FALSE(ui.AcceptDragDropPayload("dd", &got));
    EXPECT_FALSE(ui.AcceptDragDropPayload("other", &got));
    ui.End();

    // Frame 4: release over db -> payload lands.
    ReleaseLeft(events);
    ui.Begin(0.016f, { 1920, 1080 });
    ui.ItemSlot("da", {}, 0, false, { 64.f, 64.f });
    EXPECT_FALSE(ui.BeginDragDropSource("dd", 7));
    ui.ItemSlot("db", {}, 0, false, { 64.f, 64.f });
    EXPECT_TRUE(ui.AcceptDragDropPayload("dd", &got));
    ui.End();
    EXPECT_EQ(got, 7u);
    EXPECT_FALSE(ui.IsDragDropActive());

    ui.UnbindEvents(events);
}

TEST(UiBaseWidgets, LongPressHeldButton)
{
    fra::UiDraw       draw;
    fra::UiContext    ui(&draw);
    fra::EventManager events;
    ui.BindEvents(events);

    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_FALSE(ui.Button("Hold", { 160.f, 40.f }));
    const auto r = ui.LastItemRect();
    ui.End();

    MoveMouse(events, r.Center().x, r.Center().y);
    PressLeft(events);
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_FALSE(ui.Button("Hold", { 160.f, 40.f }));
    EXPECT_FALSE(ui.IsItemLongPressed(0.6f));
    ui.End();

    bool longPressed = false;
    for (int frame = 0; frame < 50; ++frame)
    {
        ui.Begin(0.016f, { 1920, 1080 });
        ui.Button("Hold", { 160.f, 40.f });
        longPressed = ui.IsItemLongPressed(0.6f);
        ui.End();
    }
    EXPECT_TRUE(longPressed);
    ReleaseLeft(events);

    ui.UnbindEvents(events);
}

TEST(UiBaseWidgets, TextInputTruncatesToMaxLen)
{
    fra::UiDraw    draw;
    fra::UiContext ui(&draw);

    std::string buf = "abcdef";
    ui.FocusTextInput("t", buf);
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_FALSE(ui.TextInput("t", buf, 4, { 200.f, 32.f }));
    ui.End();
    EXPECT_EQ(buf, "abcd");
}

TEST(UiBaseWidgets, ComboBoxDropdownShowsItemText)
{
    // Regression: dropdown rows rendered the internal "id##index" label
    // (GameUiShowcase showed "input_class##0") instead of the item text.
    // The font stub emits one kUiFlagSdfGlyph quad per character, so the
    // glyph count pins the rendered strings: box "A" (1) + chevron (1) +
    // rows "A" (1) + "BB" (2) + "CCCC" (4) = 9. Pre-fix rows were
    // "cb##0/1/2" (5 chars each) for a total of 17.
    fra::UiDraw       draw;
    fra::UiContext    ui(&draw);
    fra::EventManager events;
    ui.BindEvents(events);

    // Stubbed Create ignores the pool; no GPU access happens.
    fra::FontAtlas font = fra::FontAtlas::Create(
        *static_cast<fra::TexturePool*>(nullptr), "stub");
    ui.Style().font = &font;

    std::array<std::string_view, 3> items { "A", "BB", "CCCC" };
    int                             sel = 0;

    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_FALSE(ui.ComboBox("cb", items, &sel, { 220.f, 32.f }));
    const auto box = ui.LastItemRect();
    ui.End();

    MoveMouse(events, box.Center().x, box.Center().y);
    ClickLeft(events);
    draw.Clear();
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_FALSE(ui.ComboBox("cb", items, &sel, { 220.f, 32.f }));
    ui.End();

    std::vector<fra::UiQuad> snap;
    draw.Snapshot(snap);
    int glyphs = 0;
    for (const auto& q : snap)
    {
        if ((q.flags & fra::kUiFlagSdfGlyph) != 0)
            ++glyphs;
    }
    EXPECT_EQ(glyphs, 9);

    ui.UnbindEvents(events);
}

TEST(UiBaseWidgets, SliderIntDrawsSingleLabel)
{
    // Regression: SliderInt drew its caption twice (once directly, once via
    // the inner SliderFloat), and composite ids leaked as titles.
    fra::UiDraw    draw;
    fra::UiContext ui(&draw);

    fra::FontAtlas font = fra::FontAtlas::Create(
        *static_cast<fra::TexturePool*>(nullptr), "stub");
    ui.Style().font = &font;

    int value = 5;
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_FALSE(ui.SliderInt("Brightness", &value, 0, 100, { 200.f, 24.f }));
    ui.End();

    std::vector<fra::UiQuad> snap;
    draw.Snapshot(snap);
    int glyphs = 0;
    for (const auto& q : snap)
    {
        if ((q.flags & fra::kUiFlagSdfGlyph) != 0)
            ++glyphs;
    }
    // "Brightness" is 10 glyphs; pre-fix it was drawn twice (20).
    EXPECT_EQ(glyphs, 10);
}

TEST(UiBaseWidgets, SpinBoxHidesInnerLabel)
{
    // Regression: SpinBox displayed its internal id ("party_size") above
    // the slider. Only the "-" / "+" button glyphs may render.
    fra::UiDraw    draw;
    fra::UiContext ui(&draw);

    fra::FontAtlas font = fra::FontAtlas::Create(
        *static_cast<fra::TexturePool*>(nullptr), "stub");
    ui.Style().font = &font;

    int value = 3;
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_FALSE(ui.SpinBox("party", &value, 0, 10, 1, { 320.f, 32.f }));
    ui.End();

    std::vector<fra::UiQuad> snap;
    draw.Snapshot(snap);
    int glyphs = 0;
    for (const auto& q : snap)
    {
        if ((q.flags & fra::kUiFlagSdfGlyph) != 0)
            ++glyphs;
    }
    // Pre-fix: "party" (5) + "-" + "+" = 7.
    EXPECT_EQ(glyphs, 2);
}

TEST(UiBaseWidgets, ColorEditShowsChannelNames)
{
    // Regression: ColorEdit displayed internal "id##cN" labels
    // (e.g. "input_tint##c0") instead of "R" / "G" / "B".
    fra::UiDraw    draw;
    fra::UiContext ui(&draw);

    fra::FontAtlas font = fra::FontAtlas::Create(
        *static_cast<fra::TexturePool*>(nullptr), "stub");
    ui.Style().font = &font;

    glm::vec3 rgb { 0.2f, 0.5f, 0.8f };
    ui.Begin(0.016f, { 1920, 1080 });
    EXPECT_FALSE(ui.ColorEdit("tint", &rgb));
    ui.End();

    std::vector<fra::UiQuad> snap;
    draw.Snapshot(snap);
    int glyphs = 0;
    for (const auto& q : snap)
    {
        if ((q.flags & fra::kUiFlagSdfGlyph) != 0)
            ++glyphs;
    }
    // Pre-fix: "tint##c0/1/2" (8 chars each) = 24.
    EXPECT_EQ(glyphs, 3);
}

TEST(UiBaseWidgets, WindowClipsOverflowContent)
{
    // Content taller than the window must carry the content-rect clip;
    // only pre-push chrome (bg, title bar, resize grip) stays unclipped.
    fra::UiDraw       draw;
    fra::UiContext    ui(&draw);
    fra::EventManager events;
    ui.BindEvents(events);

    fra::UiWindowOpts opts {};
    opts.defaultPos = { 100.f, 100.f };

    ui.Begin(0.016f, { 1920, 1080 });
    ASSERT_TRUE(ui.BeginWindow("clipw", "Clipped", { 300.f, 200.f }, opts));
    for (int i = 0; i < 5; ++i)
        ui.Button(std::string("over_") + std::to_string(i), { 260.f, 40.f });
    ui.EndWindow();
    ui.End();

    const fra::UiRect        content { 112.f, 142.f, 276.f, 146.f };
    std::vector<fra::UiQuad> snap;
    draw.Snapshot(snap);
    int flagged   = 0;
    int unflagged = 0;
    for (const auto& q : snap)
    {
        if ((q.flags & fra::kUiFlagClipRect) != 0)
        {
            ++flagged;
            EXPECT_NEAR(q.clipRect.x, content.x, 1e-3f);
            EXPECT_NEAR(q.clipRect.y, content.y, 1e-3f);
            EXPECT_NEAR(q.clipRect.z, content.w, 1e-3f);
            EXPECT_NEAR(q.clipRect.w, content.h, 1e-3f);
        }
        else
            ++unflagged;
    }
    EXPECT_EQ(flagged, 5);
    EXPECT_EQ(unflagged, 3); // bg + title bar + resize grip

    ui.UnbindEvents(events);
}

TEST(UiBaseWidgets, ScrolledOutContentNotHittable)
{
    // Items scrolled outside the view are clipped visually and reject hits.
    fra::UiDraw       draw;
    fra::UiContext    ui(&draw);
    fra::EventManager events;
    ui.BindEvents(events);

    ui.Begin(0.016f, { 1920, 1080 });
    ASSERT_TRUE(ui.BeginScrollView("clipscroll", { 200.f, 100.f }, 1000.f));
    EXPECT_FALSE(ui.Button("visible", { 180.f, 30.f }));
    ui.ProgressBar(0.5f, { 180.f, 400.f });
    EXPECT_FALSE(ui.Button("below", { 180.f, 30.f }));
    const auto below = ui.LastItemRect();
    ui.EndScrollView();
    ui.End();
    EXPECT_GT(below.y, 100.f);

    // The raw rect is far below the 100px view: clicking there must miss.
    MoveMouse(events, below.Center().x, below.Center().y);
    ClickLeft(events);
    ui.Begin(0.016f, { 1920, 1080 });
    ASSERT_TRUE(ui.BeginScrollView("clipscroll", { 200.f, 100.f }, 1000.f));
    ui.Button("visible", { 180.f, 30.f });
    ui.ProgressBar(0.5f, { 180.f, 400.f });
    EXPECT_FALSE(ui.Button("below", { 180.f, 30.f }));
    EXPECT_FALSE(ui.IsItemHovered());
    ui.EndScrollView();
    ui.End();

    std::vector<fra::UiQuad> snap;
    draw.Snapshot(snap);
    bool found = false;
    for (const auto& q : snap)
    {
        if ((q.flags & fra::kUiFlagClipRect) != 0)
        {
            found = true;
            EXPECT_NEAR(q.clipRect.x, 0.f, 1e-3f);
            EXPECT_NEAR(q.clipRect.y, 0.f, 1e-3f);
            EXPECT_NEAR(q.clipRect.z, 200.f, 1e-3f);
            EXPECT_NEAR(q.clipRect.w, 100.f, 1e-3f);
        }
    }
    EXPECT_TRUE(found);

    ui.UnbindEvents(events);
}

TEST(UiBaseWidgets, OverlayBypassesWindowClip)
{
    // Tooltip content renders in the overlay list: no window clip flag,
    // even though the anchor widget is clipped.
    fra::UiDraw       draw;
    fra::UiContext    ui(&draw);
    fra::EventManager events;
    ui.BindEvents(events);

    fra::UiWindowOpts opts {};
    opts.defaultPos = { 100.f, 100.f };

    fra::UiRect tipBtn {};
    for (int frame = 0; frame < 30; ++frame)
    {
        ui.Begin(0.016f, { 1920, 1080 });
        ASSERT_TRUE(ui.BeginWindow("tipw", "Tips", { 300.f, 200.f }, opts));
        ui.ItemSlot("tips", {}, 0, false, { 64.f, 64.f });
        const auto slot = ui.LastItemRect();
        if (frame == 0)
            MoveMouse(events, slot.Center().x, slot.Center().y);
        if (ui.IsItemHovered() && ui.BeginTooltip("tiptip"))
        {
            ui.Button("tipbtn", { 100.f, 28.f });
            tipBtn = ui.LastItemRect();
            ui.EndTooltip();
        }
        ui.EndWindow();
        ui.End();
    }

    std::vector<fra::UiQuad> snap;
    draw.Snapshot(snap);
    bool flaggedTip   = false;
    bool unflaggedTip = false;
    bool flaggedSlot  = false;
    for (const auto& q : snap)
    {
        const bool inTip =
            q.rect.x >= tipBtn.x - 1.f && q.rect.x < tipBtn.x + tipBtn.w;
        if ((q.flags & fra::kUiFlagClipRect) != 0)
        {
            flaggedSlot = true;
            if (inTip)
                flaggedTip = true;
        }
        else if (inTip)
            unflaggedTip = true;
    }
    EXPECT_TRUE(flaggedSlot);
    EXPECT_FALSE(flaggedTip);
    EXPECT_TRUE(unflaggedTip);

    ui.UnbindEvents(events);
}

TEST(UiBaseWidgets, DisplayWidgetsEmitQuads)
{
    fra::UiDraw    draw;
    fra::UiContext ui(&draw);

    ui.Begin(0.016f, { 1920, 1080 });
    ui.Heading("Title");
    ui.Bullet("point");
    ui.LabelColored("colored", { 1.f, 0.f, 0.f, 1.f });
    ui.TextWrapped("wrapped paragraph for the hub page", 400.f);
    ui.Separator();
    ui.ProgressBar(0.5f, { 200.f, 16.f });
    ui.ProgressBar(1.5f, { 200.f, 16.f });
    ui.ProgressBar(-0.5f, { 200.f, 16.f });
    ui.Spinner("sp", { 24.f, 24.f });
    ui.Image(fra::TextureHandle {}, glm::vec2 { 64.f, 64.f });
    EXPECT_NEAR(ui.LastItemRect().w, 64.f, 1e-3f);
    EXPECT_NEAR(ui.LastItemRect().h, 64.f, 1e-3f);
    ui.Background({}, fra::UiImageFit::Cover, { 1.f, 1.f, 1.f, 1.f });
    ui.ShowToast("hello", 5.f);
    ui.End();

    std::vector<fra::UiQuad> snap;
    draw.Snapshot(snap);
    EXPECT_FALSE(snap.empty());
}

TEST(UiBaseWidgets, StyleVarPushPop)
{
    fra::UiDraw    draw;
    fra::UiContext ui(&draw);

    const float base = ui.Style().Var(fra::UiVar::FontSize);
    ui.PushStyleVar(fra::UiVar::FontSize, 30.f);
    EXPECT_NEAR(ui.Style().Var(fra::UiVar::FontSize), 30.f, 1e-5f);
    ui.PopStyleVar();
    EXPECT_NEAR(ui.Style().Var(fra::UiVar::FontSize), base, 1e-5f);
}
