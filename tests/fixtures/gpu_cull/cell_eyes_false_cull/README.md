# CellBulbasaur eye false-cull regression

- **Inputs:** `cull_dumps/20260907_130957` (camera + Hi-Z where eyes vanished)
- **Expected:** survivors from `cull_dumps/20260907_131005` (eyes visible)

Eye submeshes: `entityId` **2** (cell) and **5** (pbr), `meshId` 1, skinned (`flags=5`).

`hiz.r32f` is ~27 MiB (3040×1710 pyramid). Required to replay occlusion.
