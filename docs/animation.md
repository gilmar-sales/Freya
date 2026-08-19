# Animation

Skinned character animation in Freya: Assimp load → local poses → `AnimGraph`
→ skin matrix palette → GPU vertex LBS. Namespace is `fra`
(`FREYA_NAMESPACE`).

```cpp
#include <Freya/Freya.hpp>   // poses, graph, bake, GpuAnimation, Renderer GPU anim
```

Mesh load entry points remain under [Assets](assets.md) (`MeshPool`). This
page is the full animation stack.

## Architecture

There is **no CPU vertex skinning**. Bind-pose vertices stay on the GPU;
shaders apply Linear Blend Skinning (4 influences) from a bone palette SSBO
(`bones[]` / `prevBones[]` for TAA).

```mermaid
flowchart LR
  load[SkinnedModel load] --> bake[Optional BakeClip]
  bake --> graph[AnimGraph tick]
  graph --> cpuPath[CPU: PoseToSkinMatrices]
  graph --> gpuPath[GPU: GpuAnimPass compute]
  cpuPath --> palette[BoneMatrixResources]
  gpuPath --> palette
  palette --> vs[Vertex LBS]
```

| Stage | Where |
|-------|--------|
| Load skeleton + clips | CPU (`MeshPool`) |
| Dense clip bake | CPU (`BakeClip`) |
| Graph time / transitions / events | CPU (`AnimGraph`) |
| Local pose + layers + look/IK | CPU **or** GPU compute |
| Skin matrices (`global * inverseBind`) | CPU upload **or** compute write |
| Vertex deformation | Always vertex shaders |

**Layer stack (both paths):** locomotion (Clip / Blend1D / Blend2D) → optional
masked overlay → optional additive → look-at / two-bone IK.

**GPU overlay contract:** at most **one** `OverrideMasked` and **one**
`Additive` slot per `GpuAnimInstance`. Extra same-mode layers are ignored on
GPU (first wins). CPU `SampleCurrent` can still stack more layers.

## Loading skinned models

```cpp
fra::SkinnedModel fox =
    meshPool->CreateSkinnedModelFromFile("./Resources/Models/Fox.glb");
// fox.meshIds, fox.skeleton, fox.clips
```

Assimp path skips `PreTransformVertices` and limits bone weights to 4.

| Type | Role |
|------|------|
| `SkinnedModel` | `meshIds` + shared `skeleton` + `clips` |
| `Skeleton` | `names`, `parents` (−1 = root), `inverseBind`, `restLocal` |
| `AnimationClip` | `name`, `duration`, `ticksPerSecond`, `channels`, `events` |
| `EvaluateSkeletonPose(skel, clip, t)` | One-liner: sample clip → skin matrices |
| `EnsureDefaultFootstepEvents(clip)` | If empty: Footstep.L/R ≈ 15% / 65% duration |

Static instances use `SceneInstanceUpload::boneOffset = fra::kNoSkin`.

## Local poses and blending

Headers: `Pose.hpp`.

**Types:** `JointTRS`, `LocalPose`, `BoneMask`, `RootMotionDelta`,
`Blend1DSample` / `Blend1DSpan`, `Blend2DSample` / `Blend2DTriangle`.

| API | Role |
|-----|------|
| `FindJointIndex` / `RestLocalPose` | Lookup / bind rest |
| `SampleClip` | Keyframe local pose |
| `BlendLocalPoses` | Crossfade |
| `BlendMasked` | Override with `BoneMask` |
| `BlendAdditive` | `(clip − rest)` (optional mask) |
| `EvaluateBlend1D` / `EvaluateBlend2D` | Blend spaces (+ phase sync helpers) |
| `LocalToGlobal` / `PoseToSkinMatrices` | FK → skin palette |
| `ExtractRootDelta` | Root motion from local pose |

`BoneMask`: `Resize`, `SetJoint`, `SetSubtree`, `Filled`.

## AnimGraph

State machine over clips / blend spaces with params, transitions, layers, and
optional baking.

### Tick API

| Method | Behavior |
|--------|----------|
| `Advance(dt, outEvents?)` | Transitions + phase/time; **no** pose |
| `SampleCurrent()` | Sample base (+ crossfade) then layers → `LocalPose` |
| `Evaluate(dt, outEvents?)` | `Advance` + `SampleCurrent` |

Crowd GPU skin typically calls `Advance` at **animation LOD Hz** with
accumulated `dt` (not every display frame). Clip markers fire at that tick
rate; pose bake stays on the GPU.

Params: `SetFloat` / `SetBool` / `SetTrigger`, `GetFloat` / `GetBool`.  
Layers: `SetLayerEnabled` / `SetLayerWeight` / `IsLayerEnabled`.

### Conditions and transitions

`AnimCondition` kinds: `FloatGreater`, `FloatLessEqual`, `BoolTrue`,
`BoolFalse`, `Trigger` (factories: `FloatGreater`, `FloatLessEqual`,
`BoolTrue`, `BoolFalse`, `OnTrigger`).

`Transition(from, to, condition, blendDuration = 0.2f)`.

### Layers

`AnimLayerMode::OverrideMasked` (`BlendMasked`) or `Additive`
(`BlendAdditive` vs rest).

GPU pack: `TryGetLayerGpuSlots(AnimLayerGpuSlots&)` — first masked + first
additive with weight &gt; 0. Legacy: `TryGetPrimaryLayerGpuSample`.

### Locomotion GPU pack

`TryGetLocoGpuSample(AnimLocoGpuSample&)` — up to three clips + barycentric
`wA`/`wB`/`wC` (Blend1D sets `wC = 0`). Prefer over
`TryGetBlend1DGpuSample`.

### Builder

```cpp
auto graph = fra::AnimGraphBuilder()
                 .SetSkeleton(&fox.skeleton)
                 .ParamFloat("Strafe", 0.f, -1.f, 1.f)
                 .ParamFloat("Speed", 0.f, 0.f, 2.f)
                 .Blend2DState("Loco", "Strafe", "Speed") // syncPhase=true
                 .AddBlend2DSample(0.f, 0.f, idle, true, 1.f, &bakeIdle)
                 .AddBlend2DSample(0.f, 2.f, run, true, 1.85f, &bakeRun)
                 .Layer("Upper", idle)
                 .LayerMasked(&upperMask, 0.85f)
                 .LayerBake(&bakeIdle)
                 .Layer("AddIdle", idle)
                 .LayerAdditive(0.55f, &upperMask)
                 .LayerBake(&bakeIdle)
                 .EnableBaking(30.f) // optional; ≤0 uses keyframe SampleClip
                 .Entry("Loco")
                 .Build();

graph.SetFloat("Speed", 1.35f);
std::vector<fra::FiredAnimationEvent> events;
auto local = graph.Evaluate(dt, &events);
```

States: **Clip**, **Blend1D**, **Blend2D** (`syncPhase` defaults true for
gait alignment).

Debug: `CaptureDebugSnapshot` / `ApplyDebugSnapshot`
(`AnimGraphDebugSnapshot`).

## Clip baking

| API | Notes |
|-----|--------|
| `BakedClip` | Frame-major `joints[frame * jointCount + j]` |
| `BakeClip(skel, clip, bakeHz = 30)` | `bakeHz` clamped ≥ 1 |
| `SampleBaked(skel, baked, t, loop)` | Lerp/slerp |
| `FreyaOptions::animBakeHz` | Default 30; builder `SetAnimBakeHz` |

GPU upload helpers: `PackBakedClips`, `PackClipJointsFloat` /
`PackClipJointsQuant`, `MakeGpuClipHeader`, `GpuClipKey` (FNV-1a 64).

## CPU skinning path

```cpp
auto local = graph.Evaluate(dt, &events);
fra::ApplyLookAt(skel, local, model, head, target, 0.75f);
fra::SolveTwoBoneIK(skel, local, model, chain, ikTarget, pole, 0.85f);
fra::CancelRootTranslationXZ(skel, local); // after consuming root motion
auto skin = fra::PoseToSkinMatrices(skel, local);
renderer->UploadBoneMatrices(skin);

uploads.push_back({
    .model = model,
    .meshId = fox.meshIds[0],
    .materialId = mat,
    .entityId = 1,
    .boneOffset = 0,
    .boneCount = fox.skeleton.JointCount(),
});
renderer->UploadSceneInstances(uploads);
```

`BoneMatrixResources` holds current + previous palettes (TAA). Default
capacity **32768** `mat4`s. Unused slots stay identity on CPU upload.

## GPU animation path

`GpuAnimPass` (frame stage before Pick/Shadow/Deferred) samples baked clips,
blends loco/layers, optional look/IK, FK, then writes skin matrices into
`BoneMatrixResources`.

### Limits

| Constant | Value |
|----------|------:|
| `kMaxJoints` | 128 |
| `kMaxInstances` | 2048 |
| `kMaxClips` | 24 |
| `kMaxBakedJointsFloat` | 65536 (48 B/joint) |
| `kMaxBakedJointsQuant` | 196608 (16 B; same VRAM as float pool) |
| `kMaxExtractJoints` | 64 |

### Setup and frame loop

```cpp
auto* gpu = renderer->GetGpuAnimPass();
gpu->UploadSkeleton(fra::PackSkeleton(skel));
gpu->EnsureClipResident(fra::GpuClipKey("Walk"), bakeWalk); // LRU cache
gpu->UploadBoneMask(upperWeights);
gpu->UploadRestJoints(...);
gpu->SetRigIndices(lookJ, ikRoot, ikMid, ikTip, rootJ);
gpu->SetCopyPrevBones(true); // sparse LOD / FiF continuity
gpu->UploadInstances(instances);
gpu->SetEnabled(true);
// Dispatch runs in the frame graph
```

After toggling `quantizeGpuAnimJoints`, call
`renderer->RebuildGpuAnimPass()` and re-upload skeleton / clips / mask / rest
/ rig.

### `GpuAnimInstance`

Per-actor job: loco `clipA/B/C`, `time*`, `wA/B/C`; flags
(`GpuAnimFlags::Loop`, `MaskedOverlay`, `Additive`, `CancelRootXZ`); mask /
additive slots; `modelWorld`; look (`lookTarget`, `lookWeight`, `lookJoint`,
`lookLocalForward`, `lookMaxYaw` / `lookMaxPitch`); IK (`ikRoot` / `Mid` /
`Tip`, `ikTarget`, `ikPole`, `ikWeight`). Disabled joint index:
`0xffffffffu`.

Joint storage: `GpuFloatJoint` (48 B) or `GpuQuantJoint` (16 B,
smallest-three quat + half floats) via `quantizeGpuAnimJoints`.

### Clip streaming

Cache of 24 slots. `EnsureClipResident` may evict **unpinned** LRU entries.
`PinClipSlot` / `TouchClipSlot` / `EvictClipSlot` / `ResetClipCache`.
Bulk `UploadBakes` fills slots 0..n−1 pinned.

### Joint extract and timing (N+1)

| API | Role |
|-----|------|
| `SetJointExtractList` / `PollJointExtract` | Async copy of a few skin mats |
| `Renderer::PollGpuAnimJointExtract` | Wrapper at Update start |
| `PollTiming` / `PollGpuAnimTiming` | Carry + bake GPU timestamps |

Extract and timing are **one frame late**. Same-frame gameplay should use
CPU FK for sockets or sync `ReadbackBones` (debug / golden only). Prefer a
handful of hero joints over full-palette readback.

`DispatchImmediate` + `ReadbackBones`: sync golden compares (Fox0).

## Animation LOD and quality

Wall-clock pose update rates, independent of display FPS. Skipped frames keep
the last skin palette; playback does not freeze when you accumulate `dt`
into `Advance`.

| Field | Role |
|-------|------|
| `enableAnimLod` | Master switch (default true) |
| `animLodHz[4]` | Near → Far Hz |
| `animLodExitDist[3]` / `animLodEnterDist[3]` | Hysteresis bands |
| `animBakeHz` | Dense bake rate |
| `quantizeGpuAnimJoints` | 16 B joint storage |

`FreyaOptionsBuilder::SetAnimationQuality(AnimationQuality)` applies
presets (`ApplyAnimationQuality`):

| Quality | LOD | Hz (N→F) | Exit dists | Enter dists | bakeHz |
|---------|-----|----------|------------|-------------|--------|
| Low | on | 30/15/8/4 | 8/18/32 | 6/14/26 | 20 |
| Medium | on | 45/22/12/6 | 12/24/42 | 10/20/36 | 30 |
| High | on | 60/30/15/8 | 20/38/55 | 17/32/48 | 30 |
| Ultra / Off | off | every frame | — | — | 30 |

Helpers: `AnimLodHz`, `AnimLodMinHz`, `ConsumeAnimLodTick(accum, dt, hz)`,
`UpdateAnimLodTier(o, tier, dist)`.

Crowd pattern: gate **both** `Advance(pendingDt)` and GPU instance upload
on the same `mustEval` / LOD tick.

## Clip events

| Type / API | Role |
|------------|------|
| `AnimationEvent` | Marker on clip (`name`, `timeSec`) |
| `FiredAnimationEvent` | Fired this tick (+ `clip*`) |
| `CollectClipEvents` / `CollectFiredClipEvents` | Manual range collect |
| `Advance` / `Evaluate` `outEvents` | Markers crossed this tick |
| `AnimEventRing` | Fixed log (default 64); `Push` / `PushAll` / `CopyChronological` |

Events are always evaluated on the CPU graph tick, never inside the compute
shader.

## Look-at, IK, root motion

Header: `Rig.hpp`.

| API | Notes |
|-----|--------|
| `ApplyLookAt(...)` | weight=1, maxYaw=1.2, maxPitch=0.8, localForward=+Z; `maxYawRad < 0` disables clamp |
| `SolveTwoBoneIK(..., TwoBoneChain, target, pole, weight)` | Analytical |
| `IntegrateRootMotion(model, delta, planar=true)` | Planar XZ + yaw |
| `CancelRootTranslationXZ` | After driving the actor transform |
| `FindRootJoint` | First parentless, or −1 |
| `DrivePlanarLocomotion(..., localForward=−Z)` | In-place clips |

GPU mirrors look/IK on each `GpuAnimInstance`; shared defaults via
`GpuAnimPass::SetRigIndices` / `SetLookClamp`.

## Debugging

| Header | Content |
|--------|---------|
| `AnimDebug.hpp` | `SkeletonDebugSnapshot`, `PoseWorldDebugSnapshot`, `AnimEventRing` |
| `AnimGraphDebug.hpp` | `AnimGraphDebugSnapshot` (params, layers, loco, apply flags) |
| `GpuAnimDebug.hpp` | `GpuAnimDebugSnapshot` (clip cache, caps, quantize) |

Overlay draw (post-composite):

```cpp
renderer->SetDebugDrawEnabled(true);
auto& dd = renderer->GetDebugDraw();
dd.DrawSkeleton(skel, local, model, { 0.2f, 0.95f, 0.45f, 1.f });
dd.DrawLookRay(skel, local, model, head, cameraPos, { 0.3f, 0.85f, 1.f, 1.f });
dd.DrawTwoBoneIk(skel, local, model, chain.root, chain.mid, chain.tip,
                 target, pole, { 1.f, 0.55f, 0.15f, 1.f },
                 { 1.f, 0.25f, 0.35f, 1.f });
```

**SkinnedFox `anim_prof`:** ~1 Hz stdout line with CPU split
(`adv_ms` / `eval_ms` / `pack_ms` / `skin_ms` / …) and GPU timestamps
(`gpu_carry_ms` / `gpu_bake_ms`), plus `gpu_inst`, LOD histogram, and
estimated `palette_MiB`.

```bash
cd build/Examples/SkinnedFox
./SkinnedFox 2>&1 | grep anim_prof
```

## SkinnedFox example

Location: `Examples/SkinnedFox/` (binary under
`build/Examples/SkinnedFox/`). Always `cd` to the binary directory before
running (relative `./Resources/...`).

- Grid **40×25** (1000 actors), Mixamo-style Idle / Walk / Run
- Blend2D `"Loco"` on `Strafe`×`Speed`; optional Upper masked + AddIdle
  additive; fox0 look/IK
- Fox.glb look axis: local **+X** (`kLookLocalForward`)

### GPU modes (`F12`)

| Mode | Behavior |
|------|----------|
| Off | Full CPU evaluate + skin + upload |
| Fox0 | Crowd CPU; fox0 also GPU + staged golden compare |
| Crowd | GPU bake for LOD-due actors; Advance at `animLodHz` |

### Controls

| Key | Action |
|-----|--------|
| F1 | Help / status |
| F2 | Cast shadows |
| F3 | Debug draw |
| F4 | AnimGraph on/off (off = rest pose) |
| F5 | Upper masked layer |
| F6 | Additive upper |
| F7 | Look-at |
| F8 | Two-bone IK |
| F9 | Toggle quantize + `RebuildGpuAnimPass` |
| F10 | Clip events / footsteps (Crowd @ `animLodHz`) |
| F11 | Cycle `AnimationQuality` |
| F12 | Cycle GPU anim mode |
| R | Root / planar locomotion drive |
| T | Stream next clip into GPU LRU ring |
| 1/2/3, Up/Down | Speed |
| Q/E | Strafe |

## Options builders

| Setter | Effect |
|--------|--------|
| `SetAnimationQuality` | LOD Hz + distance bands + bake Hz |
| `SetAnimLodEnabled` | Toggle distance LOD |
| `SetAnimBakeHz` | Dense bake rate |
| `SetQuantizeGpuAnimJoints` | 16 B vs 48 B clip joints |

Renderer helpers: `UploadBoneMatrices`, `GetGpuAnimPass`,
`RebuildGpuAnimPass`, `DispatchGpuAnimImmediate`, `ReadbackGpuAnimBones`,
`SetGpuAnimJointExtract`, `PollGpuAnimJointExtract`, `PollGpuAnimTiming`.

## Shaders

| Path | Role |
|------|------|
| `Shaders/Anim/skin_bake.comp` | Float joint bake |
| `Shaders/Anim/skin_bake_quant.comp` | Quantized joints (`FREYA_JOINT_QUANT`) |
| `Shaders/Anim/skin_bake_body.inc` | Shared compute body |
| `Shaders/DeferredCompressed/gbuffer.vert` (etc.) | Vertex LBS from `BoneBuffer` |

Build compiles these via `cmake/CompileShaders.cmake` (do not invoke `glslc`
by hand).

## File map

| Path | Responsibility |
|------|----------------|
| `include/Freya/Asset/Skeleton.hpp` | Joint hierarchy |
| `include/Freya/Asset/AnimationClip.hpp` | Clips, events |
| `include/Freya/Asset/SkinnedModel.hpp` | Load result |
| `include/Freya/Asset/Pose.hpp` | Local pose + blend spaces |
| `include/Freya/Asset/Rig.hpp` | Look, IK, root / planar drive |
| `include/Freya/Asset/BakedAnimation.hpp` | CPU bake table |
| `include/Freya/Asset/AnimGraph.hpp` | Graph + builder + GPU sample packs |
| `include/Freya/Asset/GpuAnimation.hpp` | Instance / flags / pack / extract |
| `include/Freya/Asset/BoneMatrixResources.hpp` | Palette SSBO |
| `include/Freya/Core/GpuAnimPass.hpp` | Compute bake pass |
| `include/Freya/FreyaOptions.hpp` | LOD / bake / quantize |
| `include/Freya/Asset/AnimDebug.hpp` | Skeleton / pose / event ring |
| `include/Freya/Asset/AnimGraphDebug.hpp` | Graph UI snapshot |
| `include/Freya/Asset/GpuAnimDebug.hpp` | Pass cache snapshot |
| `include/Freya/Asset/MeshPool.hpp` | `CreateSkinnedModelFromFile` |
| `Examples/SkinnedFox/` | End-to-end reference |

## Design notes

- Prefer CPU FK / proxies for gameplay sockets; do not sync-read the full GPU
  palette every frame.
- Hero / cutscene actors that need stacked layers beyond the dual GPU slot
  should stay on the CPU skin path.
- Sparse Crowd updates require `SetCopyPrevBones(true)` so FiF slots do not
  flicker for actors not dispatched this frame.
