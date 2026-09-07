# Near-camera ground plane false-cull regression

- **Bug dump:** `cull_dumps/20260907_153341` (entityId 1 missing)
- **Good dump:** `cull_dumps/20260907_153340` (same camera/instances; plane survived)
- **Expected:** ground plane `entityId` **1** must always survive

Ground mesh: `meshId` 0, flat AABB `y=0`, `flags=1` (not skinned).
