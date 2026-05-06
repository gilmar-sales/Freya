#include <Freya/Freya.hpp>

#include <cmath>

class MainApp final : public fra::AbstractApplication
{
  public:
    explicit MainApp(const Ref<skr::ServiceProvider>& serviceProvider) :
        AbstractApplication(serviceProvider)
    {
        mMeshPool     = serviceProvider->GetService<fra::MeshPool>();
        mTexturePool  = serviceProvider->GetService<fra::TexturePool>();
        mMaterialPool = serviceProvider->GetService<fra::MaterialPool>();
        mLightService = serviceProvider->GetService<fra::LightService>();
    }

    void StartUp() override
    {
        mRenderer->ClearProjections();

        mModelMatrix[0] = glm::scale(
            glm::translate(glm::mat4(1), glm::vec3(-3, 2, 0)), glm::vec3(0.3));

        mModelMatrix[1] = glm::scale(
            glm::translate(glm::mat4(1), glm::vec3(3, 2, 0)), glm::vec3(0.3));

        mModelMatrix[2] = glm::scale(
            glm::translate(glm::mat4(1), glm::vec3(-3, -6, 0)), glm::vec3(28));

        mModelMatrix[3] = glm::scale(
            glm::translate(glm::mat4(1), glm::vec3(3, -6, 0)), glm::vec3(28));

        mSofaAlbedo = mTexturePool->CreateTextureFromFile(
            "./Resources/Textures/industrial_pipe_lamp_diff.jpg");
        mSofaNormal = mTexturePool->CreateTextureFromFile(
            "./Resources/Textures/industrial_pipe_lamp_nor_gl.png");
        mSofaRoughness = mTexturePool->CreateTextureFromFile(
        "./Resources/Textures/industrial_pipe_lamp_rough.png");
        mSofaEmissive = mTexturePool->CreateTextureFromFile(
        "./Resources/Textures/industrial_pipe_lamp_emission.png");
        mSofaMetalness = mTexturePool->CreateTextureFromFile(
            "./Resources/Textures/industrial_pipe_lamp_metal.png");

        mSofaMaterial =
            mMaterialPool->Create({ mSofaAlbedo, mSofaNormal, mSofaRoughness, mSofaEmissive, mSofaMetalness });

        mSofaModel =
            mMeshPool->CreateMeshFromFile("./Resources/Models/industrial_pipe_lamp.glb");

        mSpaceShipAlbedo = mTexturePool->CreateTextureFromFile(
            "./Resources/Textures/SpaceShip_Base_color.jpg");

        mSpaceShipNormal = mTexturePool->CreateTextureFromFile(
            "./Resources/Textures/SpaceShip_Normal.jpg");

        mSpaceShipRoughness = mTexturePool->CreateTextureFromFile(
            "./Resources/Textures/SpaceShip_Roughness.jpg");

        mSpaceShipMaterial = mMaterialPool->Create(
            { mSpaceShipAlbedo, mSpaceShipNormal, mSpaceShipRoughness });

        mSpaceShipModel =
            mMeshPool->CreateMeshFromFile("./Resources/Models/SpaceShip.fbx");

        // Add 4 point lights with different colors and intensities
        mLights.resize(4);
        mLights[0].speed        = 1.0f;
        mLights[0].phaseOffset  = 0.0f;
        mLights[0].radiusOffset = 0.0f;
        mLightService->AddLight(fra::Light {
            glm::vec3(0.0f, 5.0f, 0.0f),
            static_cast<float>(fra::LightType::Point),
            glm::vec3(1.0f, 1.0f, 1.0f), 50.0f, glm::vec3(0.0f, -1.0f, 0.0f),
            0.9f, 0.8f, 0.2f });

        mLights[1].speed        = 1.2f;
        mLights[1].phaseOffset  = 1.5f;
        mLights[1].radiusOffset = 1.0f;
        mLightService->AddLight(fra::Light {
            glm::vec3(0.0f, 5.0f, 0.0f),
            static_cast<float>(fra::LightType::Point),
            glm::vec3(1.0f, 1.0f, 1.0f), 50.0f, glm::vec3(0.0f, -1.0f, 0.0f),
            0.9f, 0.8f, 0.2f });

        mLights[2].speed        = 0.8f;
        mLights[2].phaseOffset  = 3.0f;
        mLights[2].radiusOffset = -1.0f;
        mLightService->AddLight(fra::Light {
            glm::vec3(0.0f, 5.0f, 0.0f),
            static_cast<float>(fra::LightType::Point),
            glm::vec3(1.0f, 1.0f, 1.0f), 50.0f, glm::vec3(0.0f, -1.0f, 0.0f),
            0.9f, 0.8f, 0.2f });

        mLights[3].speed        = 1.5f;
        mLights[3].phaseOffset  = 4.5f;
        mLights[3].radiusOffset = 2.0f;
        mLightService->AddLight(fra::Light {
            glm::vec3(0.0f, 5.0f, 0.0f),
            static_cast<float>(fra::LightType::Point),
            glm::vec3(1.0f, 1.0f, 1.0f), 50.0f, glm::vec3(0.0f, -1.0f, 0.0f),
            0.9f, 0.8f, 0.2f });
    }

    void Update() override
    {
        mCurrentTime += mWindow->GetDeltaTime();

        // Update lights with random orbital movement
        for (std::uint32_t i = 0; i < mLightService->GetLightCount(); ++i)
        {
            auto& light  = mLights[i];
            float offset = light.phaseOffset;
            float radius = 8.0f + light.radiusOffset;

            // Orbital movement with noise-based variation
            float x = radius * std::cos(light.speed * mCurrentTime + offset);
            float z =
                radius * std::sin(light.speed * mCurrentTime + offset * 1.3f);
            float y =
                3.0f +
                2.0f * std::sin(light.speed * 0.7f * mCurrentTime + offset);

            // Update light position in service and local cache
            mLights[i].position = glm::vec3(x, y, z);
            mLightService->UpdateLightPosition(i, glm::vec3(x, y, z));
        }

        mRenderer->BeginFrame(); // subpass 0 (depth pre-pass in deferred)

        // Orbit camera around the origin
        constexpr float radius = 15.0f;
        constexpr float speed  = 0.3f;
        const glm::vec3 position(radius * std::cos(speed * mCurrentTime), 2.0f,
                                 radius * std::sin(speed * mCurrentTime));
        mRenderer->UpdateCamera(
            position, { 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f });

        if (mInstanceMatrixBuffers == nullptr)
            mInstanceMatrixBuffers =
                mRenderer->GetBufferBuilder()
                    .SetData(&mModelMatrix[0][0])
                    .SetSize(sizeof(glm::mat4) * 4)
                    .SetUsage(fra::BufferUsage::Instance)
                    .Build();
        else
            mInstanceMatrixBuffers->Copy(
                &mModelMatrix[0][0], sizeof(glm::mat4) * 4);

        if (mRenderer->IsDeferred())
        {
            // Subpass 0: depth pre-pass (only writes depth, no material
            // needed)
            mRenderer->BindBuffer(mInstanceMatrixBuffers);
            //
            // for (const auto& mesh : mSpaceShipModel)
            //     mMeshPool->DrawInstanced(mesh, 2);
            for (const auto& mesh : mSofaModel)
                mMeshPool->DrawInstanced(mesh, 2, 2);

            // Subpass 1: G-buffer (writes position, normal, albedo)
            mRenderer->AdvanceSubpass(fra::DeferredGBufferPass);
        }

        // Draw with materials:
        //   - Deferred: subpass 1 (G-buffer)
        //   - Forward:  single subpass (albedo, normal, roughness)
        mRenderer->BindBuffer(mInstanceMatrixBuffers);
        //
        // mMaterialPool->Bind(mSpaceShipMaterial);
        // for (const auto& mesh : mSpaceShipModel)
        //     mMeshPool->DrawInstanced(mesh, 2);

        mMaterialPool->Bind(mSofaMaterial);
        for (const auto& mesh : mSofaModel)
            mMeshPool->DrawInstanced(mesh, 2, 2);

        // EndFrame advances to lighting → translucent → composite
        // and draws the fullscreen triangles for lighting + composite.
        mRenderer->EndFrame();
    }

  private:
    struct AnimatedLight
    {
        glm::vec3 position     = glm::vec3(0.0f);
        float     speed        = 1.0f;
        float     radiusOffset = 0.0f;
        float     phaseOffset  = 0.0f;
    };

    std::vector<unsigned> mSofaModel;
    std::uint32_t         mSofaAlbedo {};
    std::uint32_t         mSofaNormal {};
    std::uint32_t         mSofaRoughness {};
    std::uint32_t         mSofaEmissive {};
    std::uint32_t         mSofaMetalness {};
    std::uint32_t         mSofaMaterial {};

    std::vector<unsigned> mSpaceShipModel;
    std::uint32_t         mSpaceShipAlbedo {};
    std::uint32_t         mSpaceShipNormal {};
    std::uint32_t         mSpaceShipRoughness {};
    std::uint32_t         mSpaceShipMaterial {};

    Ref<fra::MaterialPool>     mMaterialPool;
    Ref<fra::TexturePool>      mTexturePool;
    Ref<fra::MeshPool>         mMeshPool;
    Ref<fra::LightService>     mLightService;
    glm::mat4                  mModelMatrix[4] {};
    float                      mCurrentTime {};
    std::vector<AnimatedLight> mLights;

    Ref<fra::Buffer> mInstanceMatrixBuffers;
};

int main(int argc, const char** argv)
{
    const auto app =
        skr::ApplicationBuilder()
            .WithExtension<fra::FreyaExtension>([](fra::FreyaExtension freya) {
                freya.WithOptions([](fra::FreyaOptionsBuilder& freyaOptions) {
                    freyaOptions.SetTitle("Sofa example")
                        .SetWidth(1920)
                        .SetHeight(1080)
                        .SetVSync(false)
                        .SetSampleCount(8)
                        .WithReverseZ()
                        .SetFullscreen(false)
                        .SetRenderingStrategy(fra::RenderingStrategy::Forward);
                });
            })
            .Build<MainApp>();

    app->Run();

    return 0;
}
