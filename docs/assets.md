# Assets

Freya manages meshes, textures, and PBR materials through pool services.

## MeshPool

```cpp
auto meshPool = serviceProvider->GetService<fra::MeshPool>();

// From file (Assimp: FBX, OBJ, …). Returns one ID per mesh in the file.
auto meshIds = meshPool->CreateMeshFromFile("./Resources/Models/MyModel.fbx");

// From memory (already CPU-side Freya vertices + uint16 indices)
std::uint32_t meshId = meshPool->CreateMesh(vertices, indices);
```

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
    .alphaCutoff     = 0.f,   // 0 = off; else discard when alpha < cutoff
});

materialPool->Update(material, updatedCreateInfo);
```

Descriptor set 1 bindings:

| Binding | Content |
|---------|---------|
| 0–4 | Combined image samplers: albedo, normal, roughness, emissive, metalness |
| 5 | `MaterialFactorsUniform` (48 bytes): factors, `aoFactor` in `emissive.w`, `alphaCutoff` |

Empty texture optionals use engine fallbacks (white or black). Alpha cutout
samples albedo alpha × `albedoFactor.a` in the G-buffer pass only.

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