# Screen UI (game HUD / menus)

Freya’s game UI is **not** Dear ImGui (ImGui stays in examples for
debug tools). Use `Renderer::GetUiContext()` (main-thread widgets) and
`GetUiDraw()` (thread-safe screen-space quads).

Drawn by the `ScreenUi` frame stage after `BillboardUi`, into the LDR
swapchain / viewport.

```cpp
auto& ui = mRenderer->GetUiContext();
ui.Style().font = &font; // FontAtlas from TexturePool
ui.Begin(dt, { width, height });

ui.BeginAnchor(fra::UiAnchor::TopLeft, { 24, 24 });
ui.Label("HP");
ui.ProgressBar(hp01, { 220, 18 });
ui.EndAnchor();

if (ui.BeginModal("pause", { 480, 320 })) {
    if (ui.Button("Resume")) { /* ... */ }
    ui.EndModal();
}

ui.End();
```

**Thread-safety:** `UiDraw::Rect` / `Image` / `Text` / `ProgressBar` may run
from workers between `BeginFrame` and `EndScene`. Interactive widgets on
`UiContext` are main-thread only. `WantCaptureMouse` / `WantTextInput`
gate camera / IME.

See the **GameUiDemo** example for inventory, dialogue, and chat recipes.
