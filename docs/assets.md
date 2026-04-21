# Assets

Freya provides a comprehensive asset management system for meshes, textures, and materials.

## MeshPool

Manages mesh resources loaded from 3D model files.

```cpp
auto meshPool = serviceProvider->GetService<fra::MeshPool>();

// Load mesh from file (supports FBX, OBJ, etc. via Assimp)
std::vector<unsigned> mesh = meshPool->CreateMeshFromFile("./Resources/Models/MyModel.fbx");

// Draw the mesh
meshPool->Draw(mesh[0]);
meshPool->DrawInstanced(mesh[0], instanceCount);
```

## TexturePool

Manages texture resources.

```cpp
auto texturePool = serviceProvider->GetService<fra::TexturePool>();

// Load texture from file
std::uint32_t textureId = texturePool->CreateTextureFromFile("./Resources/Textures/mytexture.png");

// Create empty texture with specified properties using ImageBuilder
std::uint32_t emptyTexture = mRenderer->GetImageBuilder()
    .SetWidth(1024)
    .SetHeight(1024)
    .SetFormat(vk::Format::eR8G8B8A8Srgb)
    .SetUsage(ImageUsage::Texture)
    .Build();
```

## MaterialPool

Manages materials combining multiple textures (albedo, normal, roughness, etc.).

```cpp
auto materialPool = serviceProvider->GetService<fra::MaterialPool>();

// Create material with texture maps
std::uint32_t material = materialPool->Create({
    albedoTextureId,    // Base color
    normalTextureId,     // Normal map
    roughnessTextureId   // Roughness map
});

// Bind material for rendering
materialPool->Bind(material);
```

## Material Structure

Materials in Freya use a PBR (Physically Based Rendering) workflow with:

| Channel | Type | Description |
|---------|------|-------------|
| Albedo | `uint32_t` | Base color texture |
| Normal | `uint32_t` | Normal map |
| Roughness | `uint32_t` | Roughness map |

## Vertex

Vertex data structure for mesh rendering.

```cpp
struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
};
```

## Asset Loading Example

```cpp
void StartUp() override
{
    // Load textures
    std::uint32_t albedo = mTexturePool->CreateTextureFromFile(
        "./Resources/Textures/MyModel_BaseColor.png");
    std::uint32_t normal = mTexturePool->CreateTextureFromFile(
        "./Resources/Textures/MyModel_Normal.png");
    std::uint32_t roughness = mTexturePool->CreateTextureFromFile(
        "./Resources/Textures/MyModel_Roughness.png");

    // Create material
    std::uint32_t material = mMaterialPool->Create({albedo, normal, roughness});

    // Load mesh
    std::vector<unsigned> mesh = mMeshPool->CreateMeshFromFile(
        "./Resources/Models/MyModel.fbx");
}

void Update() override
{
    mRenderer->BeginFrame();

    mMaterialPool->Bind(material);

    for (const auto& submesh : mesh)
    {
        mMeshPool->Draw(submesh);
    }

    mRenderer->EndFrame();
}
```
