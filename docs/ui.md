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

## 3D model preview (`UiModelPreview`)

Embed a deferred + shadows (+ skinned) panel without
`SetViewportTarget` (that path redirects the whole scene for ImGui).

1. Create `fra::UiModelPreview(windowServices, *texturePool, {512,512})`.
2. Fill `PreviewScene()` (skinned instances use `boneOffset` /
   `UploadBoneMatrices` on the shared `BoneMatrixResources`).
3. `renderer->AddModelPreview(preview)` — `ModelPreviewFrameStage` runs
   **before** `ScreenUi`.
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
| `AddModelPreview` / `Remove` | Mutate outside `Execute` (e.g. startup / shutdown) |

## Thread-safety (widgets)

`UiDraw::Rect` / `Image` / `Text` / `ProgressBar` / `CooldownRadial` may
run from workers between `BeginFrame` and `EndScene`. Interactive
widgets on `UiContext` are main-thread only. `WantCaptureMouse` /
`WantTextInput` gate camera / IME.

See **GameUiDemo** for inventory paper-doll, HUD portrait snapshot,
dialogue, chat, and ability bar (radial cooldown) recipes.
