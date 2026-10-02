# Examples

Freya includes example applications demonstrating various engine features.
Shared helpers live in `Examples/Common/` (`FreyaExamplesCommon`).

## IndustrialPipeLamp

Location: `Examples/IndustrialPipeLamp/`

Deferred PBR reference: lamps, animated lights, shadow-caster modes
(0–4), light gizmos (F3), and a secondary window (F10). Quality /
deferred views live in the debug overlay (F1).

```bash
cd build/Examples/IndustrialPipeLamp
./IndustrialPipeLamp
```

## SsaoDebug

Location: `Examples/SsaoDebug/`

SSAO debug scene (DamagedHelmet, Dragon, ally_ship). Quality, deferred
views, and SSAO knobs live in the debug overlay (F1).

```bash
cd build/Examples/SsaoDebug
./SsaoDebug
```

## SkinnedFox

Location: `Examples/SkinnedFox/`

Crowd skinned demo: Blend2D locomotion, layers, look/IK, CPU or GPU skin
paths, animation LOD, and `anim_prof` metrics. See
[Animation](animation.md#skinnedfox-example).

```bash
cd build/Examples/SkinnedFox
./SkinnedFox
```

## CellBulbasaur

Location: `Examples/CellBulbasaur/`

Cell + edge post-process, custom G-buffer techniques (cell / triplanar /
unlit), and toggleable posts (`outline`, `color_grade`, `underwater`,
`heat_haze`, `glow`). World-space `HealthBar` / `Text` nameplates demo
`BillboardAlign::Screen` (Cell) vs `Cylindrical` (PBR). Hotkeys: `F4`–`F11`.
Ground uses a tiled albedo for triplanar (`F10`). TAA/bloom stay available
from options.

```bash
cd build/Examples/CellBulbasaur
./CellBulbasaur
```

## GameUiShowcase

Location: `Examples/GameUiShowcase/`

Native Freya screen UI (not ImGui): HUD with player portrait (static
`CaptureSnapshot` from the paper-doll) beside HP, ability bar (**1–6**,
radial cooldown + tooltip), plus **F1** inventory with a deferred
skinned paper-doll (`UiModelPreview`, LMB orbit / auto-rotate, six equip
slots + bag DnD, **Tirar foto**), **F2** dialogue, **F3** chat, and the
**F4** showcase hub — a floating window with Basics / Inputs / Layout /
Windows / Game tabs covering every widget (buttons, checkboxes, sliders,
radio, toggle, spinbox, combobox, color edit, search, text input,
rows/margins/center/stacks, switcher, wizard, drawers, paginate, file
dialog, lists, grids, drag-drop, tooltips, popups, toasts, badges).
**Esc** closes the active panel. FlyCam respects `WantCaptureMouse`.
Fox.glb is copied from SkinnedFox; font is Noto Sans under
`Resources/Fonts/`.

```bash
cd build/Examples/GameUiShowcase
./GameUiShowcase
```

## BillboardShowcase

Location: `Examples/BillboardShowcase/`

Billboard types row (screen, cylindrical, spherical, fixed-axis, planar,
screen-size, velocity-stretch, soft-particle) plus player/enemy nameplates
with HP bars and animated damage numbers. Shares the Noto Sans font from
`GameUiShowcase/Resources/Fonts/` (copied at configure time).

```bash
cd build/Examples/BillboardShowcase
./BillboardShowcase
```

## ParticleShowcase

Location: `Examples/ParticleShowcase/`

`ParticleEmitter` / `RibbonEmitter` features: campfire, magic portal,
fountain, sparkle ring (flipbook atlas), one-shot `Burst()` on **Space**,
ribbon trails, grinder sparks, bullet traces, and velocity-stretched rain.

```bash
cd build/Examples/ParticleShowcase
./ParticleShowcase
```

## ChurchShowcase

Location: `Examples/ChurchShowcase/`

Gothic nave demonstrating opaque / transparent shadow interaction:
directional sun, pillar shadow bars, stained-glass windows (WBOIT), candle
point-lights, and an altar spot light. Hotkeys: `F3` light gizmos,
`1` sun / `2` altar / `3` all.

```bash
cd build/Examples/ChurchShowcase
./ChurchShowcase
```

## Creating a new example

1. Add `Examples/<Name>/Main.cpp` and an `Examples/<Name>/Resources/`
   directory as needed. `Resources/` is mandatory when the example loads
   shaders, textures, models, or fonts: `add_freya_example` copies it into
   the example binary directory at configure time
   (`file(COPY Resources DESTINATION <bindir>)`), alongside the compiled
   shaders (`Resources/Shaders/`) and the selected IBL environment
   (`Resources/Environments/`, unless `IBL none`).
2. Add a short `CMakeLists.txt`:

```cmake
add_freya_example(MyExample SOURCES Main.cpp)
# optional: IBL studio|outdoor|both|none  (default: studio)
```

3. Register it in [Examples/CMakeLists.txt](../Examples/CMakeLists.txt) with
   `add_subdirectory(MyExample)`.

Reuse helpers from `FreyaExamples::`:

- `FlyCam` — RMB look + WASD (options for flatten / look-gated move)
- `CreateGroundPlane` — procedural ground quad
- `FindClipContaining` — animation clip name search
- `CycleQuality` / `QualityName` — Low→…→Off quality enums
- `DebugOverlay` — panel built on the engine UI (`fra::UiContext`; quality /
  SSAO debug views / CPU + per-stage GPU ms via Vulkan timestamps). Toggle
  with F1; scales with resolution like the game UI; needs
  `Resources/Fonts/NotoSans-Regular.ttf` (copied by `add_freya_example()`).
- `ConfigureLogging` — console + `*.log` file sink (`FREYA_LOG_FILE`,
  `FREYA_LOG_CONSOLE=0`)

## Debug overlay

All eight examples enable `FreyaExamples::DebugOverlay` on startup. The panel
shows:

- **Timing** — CPU frame/update ms, render resolution, and GPU ms per
  `IFrameStage` from `Renderer::PollFrameGpuTiming` (Vulkan timestamps on
  desktop). This is **not** Mali HWCPipe (`PTILES` / late-ZS); those
  counters are Arm-only.
- **Quality** — Shadow / SSAO / TAA / Bloom / VSync
- **Debug views** — deferred G-buffer / SSAO / shadows (`DeferredDebugView`),
  debug draw, SSAO knobs

The overlay records into the renderer's shared UI draw queue on top of the
scene (no offscreen viewport target needed). FlyCam ignores mouse look while
the overlay wants mouse capture; WASD stays available.

## Running Examples

To build and run examples:

```bash
# Build from project root
cmake -B build -S . -G Ninja -DFREYA_BUILD_EXAMPLES=ON
cmake --build build

# Run an example (cwd must be the binary directory)
cd build/Examples/SkinnedFox && ./SkinnedFox
```
