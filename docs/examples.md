# Examples

Freya includes example applications demonstrating various engine features.

## Sofa Example

Location: `Examples/Sofa/`

A basic example demonstrating:
- Application setup
- Mesh and texture loading
- Material creation
- Instanced rendering

```bash
cd Examples/Sofa
mkdir build && cd build
cmake .. -DFREYA_BUILD_EXAMPLES=ON
cmake --build .
./Sofa
```

### Code Overview

```cpp
class MainApp final : public fra::AbstractApplication
{
  public:
    explicit MainApp(const skr::Arc<skr::ServiceProvider>& serviceProvider)
        : AbstractApplication(serviceProvider)
    {
        mMeshPool     = serviceProvider->GetService<fra::MeshPool>();
        mTexturePool  = serviceProvider->GetService<fra::TexturePool>();
        mMaterialPool = serviceProvider->GetService<fra::MaterialPool>();
    }

    void StartUp() override
    {
        // Load textures
        mSofaAlbedo = mTexturePool->CreateTextureFromFile(
            "./Resources/Textures/OfficeSofa_BaseColor.png");
        mSofaNormal = mTexturePool->CreateTextureFromFile(
            "./Resources/Textures/OfficeSofa_Normal.png");
        mSofaRoughness = mTexturePool->CreateTextureFromFile(
            "./Resources/Textures/OfficeSofa_Roughness.png");

        // Create material
        mSofaMaterial = mMaterialPool->Create({
            mSofaAlbedo, mSofaNormal, mSofaRoughness });

        // Load mesh
        mSofaModel = mMeshPool->CreateMeshFromFile(
            "./Resources/Models/OfficeSofa.fbx");
    }

    void Update() override
    {
        mRenderer->BeginFrame();

        mMaterialPool->Bind(mSofaMaterial);

        for (const auto& mesh : mSofaModel)
        {
            mMeshPool->Draw(mesh);
        }

        mRenderer->EndFrame();
    }
};
```

### Instanced Rendering

Upload current models; Freya tracks the previous frame for TAA:

```cpp
glm::mat4 models[2] = { /* ... */ };
mRenderer->SetInstanceModels(models, 2);

for (const auto& mesh : mSofaModel)
    mRenderer->DrawInstanced(mesh, mSofaMaterial, 2, 0);
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

Fullscreen cell + edge shader (`FullscreenEffect`) on a skinned Bulbasaur
GLB (idle clip). Bound to the model materials (`BindMaterial`); the ground
stays PBR. Textures are albedo maps per submesh. `F4` toggles the effect.
TAA and bloom start off so outlines stay sharp.

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
