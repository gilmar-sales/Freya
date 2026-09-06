# Examples

Freya includes example applications demonstrating various engine features.
Shared helpers live in `Examples/Common/` (`FreyaExamplesCommon`).

## IndustrialPipeLamp

Location: `Examples/IndustrialPipeLamp/`

Deferred PBR reference: lamps, animated lights, shadow-caster modes,
quality cycling (F5–F8), light gizmos, and a secondary window (F10).

```bash
cd build/Examples/IndustrialPipeLamp
./IndustrialPipeLamp
```

## SsaoDebug

Location: `Examples/SsaoDebug/`

SSAO debug scene (DamagedHelmet, Dragon, ally_ship) with view/quality
cycling and parameter nudging.

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
`heat_haze`, `glow`). Hotkeys: `F4`–`F11`. Ground uses a tiled albedo for
triplanar (`F10`). TAA/bloom stay available from options.

```bash
cd build/Examples/CellBulbasaur
./CellBulbasaur
```

## Creating a new example

1. Add `Examples/<Name>/Main.cpp` and `Resources/` as needed.
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
- `DebugOverlay` — Dear ImGui panel (quality / SSAO debug views /
  CPU + per-stage GPU ms via Vulkan timestamps)

## Debug overlay

All four examples enable `FreyaExamples::DebugOverlay` on startup. The panel
shows:

- **Timing** — CPU frame/update ms, render resolution, and GPU ms per
  `IFrameStage` from `Renderer::PollFrameGpuTiming` (Vulkan timestamps on
  desktop). This is **not** Mali HWCPipe (`PTILES` / late-ZS); those
  counters are Arm-only.
- **Quality** — Shadow / SSAO / TAA / Bloom / VSync
- **Debug views** — SSAO Lit/Blurred/Raw, shadow debug, debug draw, SSAO knobs

The scene is rendered to an offscreen viewport (`SetViewportTarget`) and
blitted behind ImGui on the swapchain UI pass (`BeginUI` / `EndUI`). FlyCam
ignores mouse look while ImGui wants mouse capture; WASD stays available.

## Running Examples

To build and run examples:

```bash
# Build from project root
cmake -B build -S . -G Ninja -DFREYA_BUILD_EXAMPLES=ON
cmake --build build

# Run an example (cwd must be the binary directory)
cd build/Examples/SkinnedFox && ./SkinnedFox
```
