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
    explicit MainApp(const Ref<skr::ServiceProvider>& serviceProvider)
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

The example also demonstrates instanced rendering:

```cpp
// Create instance buffer
mInstanceMatrixBuffers = mRenderer->GetBufferBuilder()
    .SetData(&mModelMatrix[0][0])
    .SetSize(sizeof(glm::mat4) * 4)
    .SetUsage(fra::BufferUsage::Instance)
    .Build();

// Draw instanced
for (const auto& mesh : mSpaceShipModel)
{
    mMeshPool->DrawInstanced(mesh, 2);  // Draw 2 instances
}
```

## Running Examples

To build and run examples:

```bash
# Build from project root
cmake -B build -DFREYA_BUILD_EXAMPLES=ON
cmake --build build

# Run an example
./build/Examples/Sofa/Sofa
```
