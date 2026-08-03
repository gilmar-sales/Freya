#include <Freya/Freya.hpp>

#include <cmath>
#include <iostream>

class MainApp final : public fra::AbstractApplication
{
  public:
    explicit MainApp(const skr::Arc<skr::ServiceProvider>& serviceProvider) :
        AbstractApplication(serviceProvider)
    {
        mMeshPool     = serviceProvider->GetService<fra::MeshPool>();
        mTexturePool  = serviceProvider->GetService<fra::TexturePool>();
        mMaterialPool = serviceProvider->GetService<fra::MaterialPool>();
        mLightService = serviceProvider->GetService<fra::LightService>();
        mFreyaOptions = serviceProvider->GetService<fra::FreyaOptions>();
    }

    void StartUp() override
    {
        mEventManager->Subscribe<fra::KeyReleasedEvent>(
            [this](const fra::KeyReleasedEvent& event) {
                if (event.key != fra::KeyCode::Tab)
                {
                    return;
                }

                const auto next = mRenderer->IsDeferred()
                                      ? fra::RenderingStrategy::Forward
                                      : fra::RenderingStrategy::Deferred;
                mRenderer->SetRenderingStrategy(next);
                updateTitle();

                std::cout << "Rendering strategy: "
                          << (mRenderer->IsDeferred() ? "Deferred" : "Forward")
                          << '\n';
            });

        updateTitle();
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

        mSofaMaterial = mMaterialPool->Create(
            { .albedo    = mSofaAlbedo,
              .normal    = mSofaNormal,
              .roughness = mSofaRoughness,
              .emissive  = mSofaEmissive,
              .metalness = mSofaMetalness });

        mSofaModel = mMeshPool->CreateMeshFromFile(
            "./Resources/Models/industrial_pipe_lamp.glb");

        mSpaceShipAlbedo = mTexturePool->CreateTextureFromFile(
            "./Resources/Textures/SpaceShip_Base_color.jpg");

        mSpaceShipNormal = mTexturePool->CreateTextureFromFile(
            "./Resources/Textures/SpaceShip_Normal.jpg");

        mSpaceShipRoughness = mTexturePool->CreateTextureFromFile(
            "./Resources/Textures/SpaceShip_Roughness.jpg");

        mSpaceShipMaterial = mMaterialPool->Create(
            { .albedo    = mSpaceShipAlbedo,
              .normal    = mSpaceShipNormal,
              .roughness = mSpaceShipRoughness });

        mSpaceShipModel =
            mMeshPool->CreateMeshFromFile("./Resources/Models/SpaceShip.fbx");

        // Classic light trio: cooler key, softer points farther from the mesh
        mLightService->AddLight(fra::MakeDirectionalLight(
            glm::vec3(-0.4f, -1.0f, -0.3f),
            glm::vec3(1.0f, 0.96f, 0.9f),
            1.0f));

        mAnimatedLights.resize(3);

        mAnimatedLights[0].speed        = 1.0f;
        mAnimatedLights[0].phaseOffset  = 0.0f;
        mAnimatedLights[0].radiusOffset = 4.0f;
        mAnimatedLights[0].kind         = AnimatedLightKind::Point;
        mAnimatedLights[0].index        = static_cast<std::uint32_t>(
            mLightService->AddLight(fra::MakePointLight(
                glm::vec3(12.0f, 6.0f, 0.0f),
                glm::vec3(1.0f, 0.45f, 0.3f),
                40.0f,
                1.8f)));

        mAnimatedLights[1].speed        = 1.2f;
        mAnimatedLights[1].phaseOffset  = 2.1f;
        mAnimatedLights[1].radiusOffset = 5.0f;
        mAnimatedLights[1].kind         = AnimatedLightKind::Point;
        mAnimatedLights[1].index        = static_cast<std::uint32_t>(
            mLightService->AddLight(fra::MakePointLight(
                glm::vec3(-12.0f, 6.0f, 0.0f),
                glm::vec3(0.3f, 0.5f, 1.0f),
                40.0f,
                1.8f)));

        mAnimatedLights[2].speed        = 0.9f;
        mAnimatedLights[2].phaseOffset  = 4.0f;
        mAnimatedLights[2].radiusOffset = 3.0f;
        mAnimatedLights[2].kind         = AnimatedLightKind::Spot;
        mAnimatedLights[2].index        = static_cast<std::uint32_t>(
            mLightService->AddLight(fra::MakeSpotLight(
                glm::vec3(0.0f, 12.0f, 10.0f),
                glm::vec3(0.0f, -1.0f, -0.5f),
                glm::vec3(0.95f, 0.95f, 1.0f),
                45.0f,
                glm::radians(10.0f),
                glm::radians(18.0f),
                3.0f)));

        // Soft rectangular fill light above the cluster
        mLightService->AddLight(fra::MakeAreaLight(
            glm::vec3(0.0f, 10.0f, 0.0f),
            glm::vec3(0.0f, -1.0f, 0.0f),
            glm::vec3(1.0f, 0.0f, 0.0f),
            4.0f,
            2.5f,
            glm::vec3(1.0f, 0.92f, 0.85f),
            6.0f));
    }

    void Update() override
    {
        mCurrentTime += mWindow->GetDeltaTime();

        for (auto& animated : mAnimatedLights)
        {
            const float offset = animated.phaseOffset;
            const float radius = 14.0f + animated.radiusOffset;

            const float x =
                radius * std::cos(animated.speed * mCurrentTime + offset);
            const float z =
                radius *
                std::sin(animated.speed * mCurrentTime + offset * 1.3f);
            const float y =
                3.0f +
                2.0f * std::sin(animated.speed * 0.7f * mCurrentTime + offset);
            const glm::vec3 position(x, y, z);

            if (animated.kind == AnimatedLightKind::Point)
            {
                mLightService->UpdateLightPosition(animated.index, position);
                continue;
            }

            const auto* current = mLightService->GetLight(animated.index);
            if (current == nullptr)
            {
                continue;
            }

            fra::Light spot = *current;
            spot.position   = position;
            // Spot aims at the origin (sofa cluster)
            const glm::vec3 toOrigin = glm::vec3(0.0f) - position;
            if (glm::length(toOrigin) > 1e-4f)
            {
                spot.direction = glm::normalize(toOrigin);
            }
            mLightService->UpdateLight(animated.index, spot);
        }

        mRenderer->BeginFrame();

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

        mRenderer->BindBuffer(mInstanceMatrixBuffers);

        // Queue draw commands (stored in Renderer, reused for depth pre-pass
        // and gbuffer)
        for (const auto& mesh : mSofaModel)
            mRenderer->DrawInstanced(mesh, mSofaMaterial, 2, 2);

        // EndFrame advances to lighting → translucent → composite
        // and draws the fullscreen triangles for lighting + composite.
        mRenderer->EndFrame();
    }

  private:
    void updateTitle()
    {
        const char* mode = mRenderer->IsDeferred() ? "Deferred" : "Forward";
        mFreyaOptions->title =
            std::string("Industrial Pipe Lamp — ") + mode + " [Tab]";
    }

    enum class AnimatedLightKind
    {
        Point,
        Spot
    };

    struct AnimatedLight
    {
        std::uint32_t     index        = 0;
        AnimatedLightKind kind         = AnimatedLightKind::Point;
        float             speed        = 1.0f;
        float             radiusOffset = 0.0f;
        float             phaseOffset  = 0.0f;
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

    skr::Arc<fra::MaterialPool> mMaterialPool;
    skr::Arc<fra::TexturePool>  mTexturePool;
    skr::Arc<fra::MeshPool>     mMeshPool;
    skr::Arc<fra::LightService> mLightService;
    skr::Arc<fra::FreyaOptions> mFreyaOptions;
    glm::mat4                   mModelMatrix[4] {};
    float                       mCurrentTime {};
    std::vector<AnimatedLight>  mAnimatedLights;

    skr::Arc<fra::Buffer> mInstanceMatrixBuffers;
};

int main(int argc, const char** argv)
{
    const auto app =
        skr::ApplicationBuilder()
            .WithExtension<fra::FreyaExtension>([](fra::FreyaExtension freya) {
                freya.WithOptions([](fra::FreyaOptionsBuilder& freyaOptions) {
                    freyaOptions
                        .SetTitle("Industrial Pipe Lamp — Forward [Tab]")
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
