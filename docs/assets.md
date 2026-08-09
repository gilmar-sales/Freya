# Assets

Freya manages meshes, textures, and PBR materials through pool services.

## MeshPool

```cpp
auto meshPool = serviceProvider->GetService<fra::MeshPool>();

// From file (Assimp: FBX, OBJ, …). Returns one ID per mesh in the file.
auto meshIds = meshPool->CreateMeshFromFile("./Resources/Models/MyModel.fbx");

// Skinned (no PreTransformVertices): joints/weights + shared skeleton/clips.
fra::SkinnedModel fox =
    meshPool->CreateSkinnedModelFromFile("./Resources/Models/Fox.glb");

// Pose API: SampleClip → Blend1D / BlendMasked → PoseToSkinMatrices.
// EvaluateSkeletonPose is a thin wrapper for a single clip.
auto skin = fra::PoseToSkinMatrices(
    fox.skeleton, fra::SampleClip(fox.skeleton, fox.clips[0], timeSec));
renderer->UploadBoneMatrices(skin);

// AnimGraph: Blend1D locomotion on a float param (Idle↔Walk↔Run).
// syncPhase=true (default) keeps feet aligned while blending gaits.
auto graph = fra::AnimGraphBuilder()
                 .SetSkeleton(&fox.skeleton)
                 .ParamFloat("Speed")
                 .Blend1DState("Loco", "Speed")
                 .AddBlendSample(0.f, fox.clips[0])
                 .AddBlendSample(1.f, fox.clips[1])
                 .AddBlendSample(2.f, fox.clips[2], true, 1.85f)
                 .Entry("Loco")
                 .Build();
graph.SetFloat("Speed", 1.35f); // continuous blend Walk↔Run
auto local = graph.Evaluate(dt);

// Bone mask layer: upper-body overlay without stomping the gait.
fra::BoneMask upper = fra::BoneMask::Filled(fox.skeleton.JointCount(), 0.f);
if (auto spine = fra::FindJointIndex(fox.skeleton, "Spine"); spine >= 0)
    upper.SetSubtree(fox.skeleton, static_cast<std::uint32_t>(spine), 1.f);
local = fra::BlendMasked(
    local, fra::SampleClip(fox.skeleton, fox.clips[0], timeSec), upper, 0.8f);

// Additive layer: (clip − rest) on top of locomotion (partial weight + mask).
local = fra::BlendAdditive(
    local, fra::SampleClip(fox.skeleton, fox.clips[0], timeSec),
    fra::RestLocalPose(fox.skeleton), upper, 0.5f);

// Rig MVP (after layers): look-at → two-bone IK → root / locomotion drive.
fra::ApplyLookAt(fox.skeleton, local, model, headJoint, cameraPos, 0.7f);
fra::SolveTwoBoneIK(fox.skeleton, local, model, legChain, footTarget, pole, 0.85f);
model = fra::DrivePlanarLocomotion(model, metersPerSec, dt);
fra::CancelRootTranslationXZ(fox.skeleton, local);
// Or from authored curves: IntegrateRootMotion(model, ExtractRootDelta(...));

renderer->UploadBoneMatrices(fra::PoseToSkinMatrices(fox.skeleton, local));
// Instances share boneOffset=0 .. JointCount()-1 in the bone palette.
uploads.push_back({
    .model = model,
    .meshId = fox.meshIds[0],
    .materialId = mat,
    .entityId = 1,
    .boneOffset = 0,
    .boneCount = fox.skeleton.JointCount(),
});

// From memory (already CPU-side Freya vertices + uint32 indices)
std::uint32_t meshId = meshPool->CreateMesh(vertices, indices);
```

Static meshes leave `Vertex::joints/weights` at defaults and
`SceneInstanceUpload::boneOffset = fra::kNoSkin`. Skinned draws evaluate a
second/prev bone palette for TAA velocity (`UploadBoneMatrices`).

Draw submission goes through `Renderer::UploadSceneInstances` (preferred) or
the legacy `Draw` / `DrawInstanced` helpers.

## TexturePool

```cpp
auto texturePool = serviceProvider->GetService<fra::TexturePool>();

std::uint32_t fromFile =
    texturePool->CreateTextureFromFile("./Resources/Textures/albedo.png");

// RGBA8 (or other channel count) already in memory
std::vector<std::uint8_t> rgba = /* ... */;
std::uint32_t fromMemory = texturePool->CreateTextureFromMemory(
    rgba.data(), width, height, /*channels=*/4);
```

Both paths create a mipmapped image and linear anisotropic sampler.

## MaterialPool

```cpp
auto materialPool = serviceProvider->GetService<fra::MaterialPool>();

std::uint32_t material = materialPool->Create(fra::MaterialCreateInfo {
    .albedo     = albedoId,
    .normal     = normalId,
    .roughness  = roughnessId,
    .emissive   = emissiveId,
    .metalness  = metalnessId,
    .albedoFactor    = { 1.f, 1.f, 1.f, 1.f },
    .roughnessFactor = 1.f,
    .metalnessFactor = 1.f,
    .emissiveFactor  = { 1.f, 1.f, 1.f },
    .aoFactor        = 1.f,   // constant AO into G-buffer
    .alphaCutoff     = 0.f,   // Mask: discard when alpha < cutoff
    .alphaMode       = fra::AlphaMode::Opaque, // Opaque | Mask | Blend
    .clearcoat          = 0.f,    // deferred GGX clearcoat weight
    .clearcoatRoughness = 0.03f,  // glTF-style default
});

materialPool->Update(material, updatedCreateInfo);
```

`AlphaMode::Opaque` / `Mask` stay in the deferred MDI camera cull. `Mask`
uses `alphaCutoff` cutout in the G-buffer. `AlphaMode::Blend` is filtered
into the Weighted Blended OIT pass (`CullMode::Translucent`); use
`albedoFactor.a` (and albedo alpha) for coverage, and typically
`castShadows = false` on glass instances.

`clearcoat` (>0) enables a second dielectric GGX lobe in deferred lighting
(F0=0.04). The weight is stored in G-buffer PBR.a; `clearcoatRoughness` is
looked up from the bindless `MaterialGPU` table via albedo.a material ID.

Descriptor set 1 bindings:

| Binding | Content |
|---------|---------|
| 0–4 | Combined image samplers: albedo, normal, roughness, emissive, metalness |
| 5 | `MaterialFactorsUniform` (48 bytes): factors, `aoFactor` in `emissive.w`, `alphaCutoff` |

Empty texture optionals use engine fallbacks (white or black). Alpha cutout
samples albedo alpha × `albedoFactor.a` in the G-buffer pass only (Mask).

## Vertex

```cpp
struct Vertex
{
    glm::vec3 position;
    glm::vec3 color;
    glm::vec3 normal;
    glm::vec3 tangent;
    glm::vec2 texCoord;
};
```

## Instancing (GPU-driven MDI)

Prefer `Renderer::UploadSceneInstances` with one record per logical instance.
Contiguous uploads that share the same `meshId` become **one** multi-draw
indirect command; frustum cull (compute) atomic-compacts visible instances.

**Contract:** sort by `(meshId, entityId)` when possible (Freya skips its
internal sort if already ordered by vertex/index chunk + mesh + entity).
TAA `prevModel` is resolved by `entityId` (first frame / new ids: `prev ==
model`).

```cpp
std::vector<fra::SceneInstanceUpload> instances;
// Prefer push order: same mesh contiguous, entityId ascending within mesh.
instances.push_back({ .model = M, .meshId = mesh, .materialId = mat,
                      .entityId = id, .castShadows = true });
mRenderer->UploadSceneInstances(instances);
```

Legacy path: `SetInstanceModels` + `Draw` / `DrawInstanced` still works and is
expanded into `UploadSceneInstances` internally.