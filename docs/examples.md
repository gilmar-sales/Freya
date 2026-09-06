# Examples

Freya includes example applications demonstrating various engine features.

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

## Running Examples

To build and run examples:

```bash
# Build from project root
cmake -B build -S . -G Ninja -DFREYA_BUILD_EXAMPLES=ON
cmake --build build

# Run an example (cwd must be the binary directory)
cd build/Examples/SkinnedFox && ./SkinnedFox
```
