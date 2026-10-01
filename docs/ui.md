# Screen UI

Freya’s game UI is **not** Dear ImGui (ImGui stays in examples for
debug tools). Use `Renderer::GetUiContext()` (main-thread widgets) and
`GetUiDraw()` (thread-safe screen-space quads).

Drawn by the `ScreenUi` frame stage after `BillboardUi`, into the LDR
swapchain / viewport.

Pointer hit-tests use framebuffer pixel space (same as `Begin`'s
`fbExtent`). When the host draws into an offscreen RT shown inside another
UI (editor viewport), remap the cursor and call
`SetPointerFramebuffer(fbX, fbY)` after `PumpEvents` / before `Begin`.

```cpp
auto& ui = mRenderer->GetUiContext();
ui.Style().font = &font; // FontAtlas from TexturePool
// Optional: ui.SetPointerFramebuffer(fbX, fbY);
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

## 3D model preview (`UiModelPreview`)

Embed a deferred + shadows (+ skinned) panel without
`SetViewportTarget` (that path redirects the whole scene for ImGui).

1. Create `fra::UiModelPreview(windowServices, *texturePool, {512,512})`.
2. Fill `PreviewScene()` (skinned instances use `boneOffset` /
   `UploadBoneMatrices` on the shared `BoneMatrixResources`).
3. `renderer->AddModelPreview(preview)` — `ModelPreviewFrameStage` runs
   **before** `ScreenUi`. Call `renderer->RemoveModelPreview(preview)`
   to detach it (e.g. on shutdown), before destroying the preview.
4. Live sample: `ui.ModelPreview(id, preview->Texture(), size, preview)`.
5. Static HUD photo: `preview->CaptureSnapshot(*pool, {96,96})` →
   owned `TextureHandle` (survives after removing the preview).

Generic static textures still use `CreateTextureFromFile` /
`CreateTextureFromMemory`.

Orbit (`UiModelPreviewOrbit`): drag button, sensitivity, yaw/pitch
clamps, distance, optional `autoRotate`. `FeedMouse*` / `SetOrbit` are
UI-thread; `Record` is render-thread only.

**Cost:** each preview owns a mini deferred/shadow/composite stack
(plus SSAO / shadow-mask / TAA / Bloom when those FreyaOptions flags are
on). Prefer one paper-doll; N stacks scale linearly.

### Thread-safety contract

| Surface | Rule |
|---------|------|
| `UiDraw::Image` (live or snapshot handle) | SpinLock; workers OK between `BeginFrame` and `EndScene` |
| `TexturePool::RegisterExternal` / `Unregister` / snapshot create | Same pool lock as `CreateTextureFromMemory`; do not race `Destroy` on the same id |
| `UiModelPreview::Record` | Frame stage / render thread only |
| Orbit / `FeedMouse*` / `SetOrbit` | Main (UI) thread; yaw/pitch read atomically in `Record` |
| `UiContext::ModelPreview` | Main-thread only |
| `CaptureSnapshot` | Main/render thread after ≥1 `Record`; GPU copy finishes before the handle is used (or use next frame) |
| `AddModelPreview` / `RemoveModelPreview` | Mutate outside `Execute` (e.g. startup / shutdown) |

## Thread-safety (widgets)

`UiDraw::Rect` / `Image` / `Text` / `ProgressBar` / `CooldownRadial` may
run from workers between `BeginFrame` and `EndScene`. Interactive
widgets on `UiContext` are main-thread only. `WantCaptureMouse` /
`WantTextInput` gate camera / IME.

See **GameUiShowcase** for inventory paper-doll, HUD portrait snapshot,
dialogue, chat, and ability bar (radial cooldown) recipes.

## Extended widgets (groups 1/2/3/6/7)

Input: `RadioButton`, `ToggleSwitch`, `SliderInt`, `SpinBox`, `ComboBox`
(overlay dropdown), `ColorEdit` (3 sliders + swatch), `SearchBox`
(`TextInput` + clear button).
Display: `Heading`, `Bullet`, `LabelColored`, `CollapsingHeader`
(persistent open state), `Spinner` (time-animated), `ShowToast`
(auto-fading overlay, drawn in `End()`).
Layout: `BeginRow`/`NextCell`/`EndRow` (weight flex), `BeginMargin`/`
EndMargin`, `BeginCenter`/`EndCenter`, `BeginVStack`/`EndVStack`
(custom `ItemSpacing`).
Interaction: `IsItemDoubleClicked` (0.4s), `IsItemLongPressed(0.6s)`,
`BeginDisabled`/`EndDisabled` (`DisabledAlpha`), internal clipboard
(`SetClipboard`/`Clipboard`, Ctrl+C/X/V in `TextInput`), cursor
Left/Right/Home/End/Delete, `IBeam` cursor on text fields, `HSize`/
`VSize` cursors available.
Style: `UiStyle::SaveIni`/`LoadIni` (`Color.*` / `Var.*` lines),
`UiVar::DisabledAlpha` / `AnimSpeed`, hover-animated `Button`
(`hoverT` lerp), `UiCol::ToastBg` / `HeaderBg`.

## Windows / navigation (group 4)

`BeginWindow` / `EndWindow` (`UiWindowOpts`): floating panel with title
bar, drag-to-move (title grab), resize grip (bottom-right, `HSize`
cursor), collapse chevron, optional close `x`. Position/size persist in
`WidgetState`; `WindowRect(id)`, `IsWindowOpen` / `SetWindowOpen`.
`BeginSwitcher` / `EndSwitcher`: fixed-size stacked page container.
`BeginWizard` / `EndWizard` + `WizardNav`: numbered step header plus
Back / Next-or-Finish row (`-1` / `+1` / `0`).
`BeginDrawer` / `EndDrawer` (`UiAnchor::Left` / `Right`): full-height
side panel with collapse strip; `IsDrawerOpen` / `OpenDrawer`.
`Paginate` (`< 1/5 >`, fixed `{220,32}` layout) + `PageRange` helper.
`OpenFileDialog` + `FileDialog` (`UiFileDialogOpts`: start directory,
extension filter, `..` entry): modal `std::filesystem` picker, single
click selects, double-click / Select confirms, Cancel / Esc aborts.
Out of scope: docking layout and multi-viewport (host-level windows
via `CreateWindow` cover this).
