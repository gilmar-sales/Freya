#include <Freya/Freya.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <unordered_set>
#include <vector>

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
        mEventManager->Subscribe<fra::KeyPressedEvent>(
            [this](const fra::KeyPressedEvent& event) {
                mKeysHeld.insert(static_cast<std::uint32_t>(event.key));
            });

        mEventManager->Subscribe<fra::KeyReleasedEvent>(
            [this](const fra::KeyReleasedEvent& event) {
                mKeysHeld.erase(static_cast<std::uint32_t>(event.key));

                if (event.key == fra::KeyCode::F5)
                {
                    cycleShadowQuality();
                    return;
                }
                if (event.key == fra::KeyCode::F6)
                {
                    cycleSsaoQuality();
                    return;
                }
                if (event.key == fra::KeyCode::F7)
                {
                    cycleTaaQuality();
                    return;
                }
                if (event.key == fra::KeyCode::F8)
                {
                    cycleBloomQuality();
                    return;
                }

                if (event.key == fra::KeyCode::Escape && mLookHeld)
                {
                    setLookHeld(false);
                    return;
                }

                if (event.key == fra::KeyCode::Num0 ||
                    event.key == fra::KeyCode::Kp0)
                {
                    setShadowCasterMode(0);
                    return;
                }
                if (event.key == fra::KeyCode::Num1 ||
                    event.key == fra::KeyCode::Kp1)
                {
                    setShadowCasterMode(1);
                    return;
                }
                if (event.key == fra::KeyCode::Num2 ||
                    event.key == fra::KeyCode::Kp2)
                {
                    setShadowCasterMode(2);
                    return;
                }
                if (event.key == fra::KeyCode::Num3 ||
                    event.key == fra::KeyCode::Kp3)
                {
                    setShadowCasterMode(3);
                    return;
                }
                if (event.key == fra::KeyCode::Num4 ||
                    event.key == fra::KeyCode::Kp4)
                {
                    setShadowCasterMode(4);
                    return;
                }
            });

        mEventManager->Subscribe<fra::MouseButtonPressedEvent>(
            [this](const fra::MouseButtonPressedEvent& event) {
                if (event.button == fra::MouseButton::Right)
                {
                    setLookHeld(true);
                }
            });

        mEventManager->Subscribe<fra::MouseButtonReleasedEvent>(
            [this](const fra::MouseButtonReleasedEvent& event) {
                if (event.button == fra::MouseButton::Right)
                {
                    setLookHeld(false);
                }
            });

        mEventManager->Subscribe<fra::MouseMoveEvent>(
            [this](const fra::MouseMoveEvent& event) {
                if (!mLookHeld)
                {
                    return;
                }

                mYaw += event.deltaX * kMouseSensitivity;
                mPitch -= event.deltaY * kMouseSensitivity;
                mPitch = std::clamp(mPitch, -89.0f, 89.0f);
            });

        updateTitle();
        mRenderer->ClearProjections();

        // Two full-size lamps on the ground plane + the plane itself.
        mModelMatrix[0] =
            glm::scale(glm::translate(glm::mat4(1), glm::vec3(-3, -6, 0)),
                       glm::vec3(28));

        mModelMatrix[1] =
            glm::scale(glm::translate(glm::mat4(1), glm::vec3(3, -6, 0)),
                       glm::vec3(28));

        // Slightly below the lamp bases so the plane is not z-fighting.
        mModelMatrix[2] = glm::translate(glm::mat4(1), glm::vec3(0, -6.05f, 0));

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

        mGroundMesh     = createGroundMesh();
        mGroundMaterial = mMaterialPool->Create({});

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

        // Fill lights stay dim so local casters dominate when diagnosed.
        // castShadows is toggled one-at-a-time (spots all share mode 4).
        {
            auto key = fra::MakeDirectionalLight(glm::vec3(-0.4f, -1.0f, -0.3f),
                                                 glm::vec3(1.0f, 0.96f, 0.9f),
                                                 0.35f);
            key.castShadows = false;
            mDirectionalIndex =
                static_cast<std::uint32_t>(mLightService->AddLight(key));
        }

        mAnimatedLights.clear();
        mSpotIndices.clear();

        {
            AnimatedLight warm {};
            warm.speed        = 1.0f;
            warm.phaseOffset  = 0.0f;
            warm.radiusOffset = 4.0f;
            warm.kind         = AnimatedLightKind::Point;
            auto point        = fra::MakePointLight(
                glm::vec3(12.0f, 6.0f, 0.0f),
                glm::vec3(1.0f, 0.45f, 0.3f),
                50.0f,
                14.0f);
            point.castShadows = false;
            warm.index =
                static_cast<std::uint32_t>(mLightService->AddLight(point));
            mWarmPointIndex = warm.index;
            mAnimatedLights.push_back(warm);
        }

        {
            AnimatedLight cool {};
            cool.speed        = 1.2f;
            cool.phaseOffset  = 2.1f;
            cool.radiusOffset = 5.0f;
            cool.kind         = AnimatedLightKind::Point;
            auto point        = fra::MakePointLight(
                glm::vec3(-12.0f, 6.0f, 0.0f),
                glm::vec3(0.3f, 0.5f, 1.0f),
                50.0f,
                14.0f);
            point.castShadows = false;
            cool.index =
                static_cast<std::uint32_t>(mLightService->AddLight(point));
            mCoolPointIndex = cool.index;
            mAnimatedLights.push_back(cool);
        }

        // Four orbiting spots exercise all shadow slots (MAX_SPOT_SHADOWS).
        struct SpotSeed
        {
            float     speed;
            float     phase;
            float     radiusOffset;
            glm::vec3 color;
        };
        const SpotSeed spotSeeds[] = {
            { 0.90f, 4.0f, 3.0f, { 0.95f, 0.95f, 1.00f } },
            { 0.75f, 0.8f, 6.0f, { 1.00f, 0.85f, 0.55f } },
            { 1.05f, 2.6f, 2.0f, { 0.55f, 0.85f, 1.00f } },
            { 0.85f, 5.2f, 7.5f, { 1.00f, 0.55f, 0.70f } },
        };
        for (const auto& seed : spotSeeds)
        {
            AnimatedLight spotAnim {};
            spotAnim.speed        = seed.speed;
            spotAnim.phaseOffset  = seed.phase;
            spotAnim.radiusOffset = seed.radiusOffset;
            spotAnim.kind         = AnimatedLightKind::Spot;

            auto spot = fra::MakeSpotLight(
                glm::vec3(0.0f, 12.0f, 10.0f),
                glm::vec3(0.0f, -1.0f, -0.5f),
                seed.color,
                55.0f,
                glm::radians(12.0f),
                glm::radians(22.0f),
                10.0f);
            spot.castShadows = false;
            spotAnim.index =
                static_cast<std::uint32_t>(mLightService->AddLight(spot));
            mSpotIndices.push_back(spotAnim.index);
            mAnimatedLights.push_back(spotAnim);
        }

        // Soft rectangular fill — keep mild so it doesn't wash out casters.
        mLightService->AddLight(fra::MakeAreaLight(
            glm::vec3(0.0f, 10.0f, 0.0f),
            glm::vec3(0.0f, -1.0f, 0.0f),
            glm::vec3(1.0f, 0.0f, 0.0f),
            4.0f,
            2.5f,
            glm::vec3(1.0f, 0.92f, 0.85f),
            1.5f));

        std::cout
            << "Controls: RMB look | WASD move | Space/Q up | Ctrl/E down | "
               "Esc release mouse\n"
            << "Shadow test: 0=all  1=directional  2=warm point  "
               "3=cool point  4=all spots\n";

        // All casters on by default (same as key 0).
        setShadowCasterMode(0);
    }

    void Update() override
    {
        const float dt = mWindow->GetDeltaTime();
        mCurrentTime += dt;
        updateCamera(dt);

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

        const glm::vec3 forward = cameraForward();
        mRenderer->UpdateCamera(mCameraPos,
                                mCameraPos + forward,
                                glm::vec3(0.0f, 1.0f, 0.0f));

        mRenderer->SetInstanceModels(mModelMatrix, kInstanceCount);

        // Two lamps cast/receive shadows onto each other and the ground.
        for (const auto& mesh : mSofaModel)
            mRenderer->DrawInstanced(mesh, mSofaMaterial, 2, 0);

        // Ground receives shadows but must not cast — a full-screen planar
        // depth write destroys cascade util and z-fights into binary noise.
        mRenderer->DrawInstanced(mGroundMesh, mGroundMaterial, 1, 2, false);

        // EndFrame advances to lighting → translucent → composite
        // and draws the fullscreen triangles for lighting + composite.
        mRenderer->EndFrame();
    }

  private:
    static constexpr std::size_t kInstanceCount    = 3;
    static constexpr float       kMoveSpeed        = 12.0f;
    static constexpr float       kMouseSensitivity = 0.12f;

    std::uint32_t createGroundMesh()
    {
        constexpr float half = 30.0f;
        const auto      tint = glm::vec3(0.72f, 0.72f, 0.76f);
        const auto      up   = glm::vec3(0.0f, 1.0f, 0.0f);
        const auto      tan  = glm::vec3(1.0f, 0.0f, 0.0f);

        const std::vector<fra::Vertex> vertices = {
            { { -half, 0.0f, -half }, tint, up, tan, { 0.0f, 0.0f } },
            { { half, 0.0f, -half }, tint, up, tan, { 1.0f, 0.0f } },
            { { half, 0.0f, half }, tint, up, tan, { 1.0f, 1.0f } },
            { { -half, 0.0f, half }, tint, up, tan, { 0.0f, 1.0f } },
        };
        // Single-sided (+Y). Two-sided coplanar indices z-fight in the CSM
        // depth map under CullBack + lightProj Y-flip.
        const std::vector<std::uint16_t> indices = {
            0, 3, 2, 0, 2, 1,
        };

        return mMeshPool->CreateMesh(vertices, indices);
    }

    [[nodiscard]] bool isHeld(fra::KeyCode key) const
    {
        return mKeysHeld.contains(static_cast<std::uint32_t>(key));
    }

    void setLookHeld(bool held)
    {
        mLookHeld = held;
        mWindow->SetMouseGrab(held);
    }

    [[nodiscard]] glm::vec3 cameraForward() const
    {
        const float yawRad   = glm::radians(mYaw);
        const float pitchRad = glm::radians(mPitch);
        return glm::normalize(glm::vec3 {
            std::cos(pitchRad) * std::cos(yawRad),
            std::sin(pitchRad),
            std::cos(pitchRad) * std::sin(yawRad),
        });
    }

    void updateCamera(float dt)
    {
        const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
        const glm::vec3 look = cameraForward();
        const glm::vec3 forward =
            glm::normalize(glm::vec3(look.x, 0.0f, look.z));
        const glm::vec3 right = glm::normalize(glm::cross(forward, worldUp));
        glm::vec3       move(0.0f);

        if (isHeld(fra::KeyCode::W))
            move += forward;
        if (isHeld(fra::KeyCode::S))
            move -= forward;
        if (isHeld(fra::KeyCode::D))
            move += right;
        if (isHeld(fra::KeyCode::A))
            move -= right;
        if (isHeld(fra::KeyCode::Space) || isHeld(fra::KeyCode::Q))
            move += worldUp;
        if (isHeld(fra::KeyCode::LCtrl) || isHeld(fra::KeyCode::RCtrl) ||
            isHeld(fra::KeyCode::E))
            move -= worldUp;

        if (glm::length(move) > 1e-4f)
            mCameraPos += glm::normalize(move) * kMoveSpeed * dt;
    }

    void setLightCastShadows(std::uint32_t index, bool enabled)
    {
        const auto* current = mLightService->GetLight(index);
        if (current == nullptr)
        {
            return;
        }

        fra::Light light  = *current;
        light.castShadows = enabled;
        mLightService->UpdateLight(index, light);
    }

    void setShadowCasterMode(int mode)
    {
        mShadowCasterMode = mode;

        const bool all = mode == 0;
        setLightCastShadows(mDirectionalIndex, all || mode == 1);
        setLightCastShadows(mWarmPointIndex, all || mode == 2);
        setLightCastShadows(mCoolPointIndex, all || mode == 3);
        for (const auto spotIndex : mSpotIndices)
            setLightCastShadows(spotIndex, all || mode == 4);

        static constexpr const char* kNames[] = {
            "all", "directional", "warm point", "cool point", "all spots",
        };
        const char* name = (mode >= 0 && mode <= 4) ? kNames[mode] : "unknown";
        std::cout << "Shadow caster: " << name << " [" << mode << "]\n";
        updateTitle();
    }

    void cycleShadowQuality()
    {
        const auto         current = mRenderer->GetShadowQuality();
        fra::ShadowQuality next    = fra::ShadowQuality::Low;
        switch (current)
        {
            case fra::ShadowQuality::Low:
                next = fra::ShadowQuality::Medium;
                break;
            case fra::ShadowQuality::Medium:
                next = fra::ShadowQuality::High;
                break;
            case fra::ShadowQuality::High:
                next = fra::ShadowQuality::Ultra;
                break;
            case fra::ShadowQuality::Ultra:
                next = fra::ShadowQuality::Low;
                break;
        }

        mRenderer->SetShadowQuality(next);

        static constexpr const char* kNames[] = { "Low", "Medium", "High",
                                                  "Ultra" };
        std::cout << "Shadow quality: " << kNames[static_cast<int>(next)]
                  << " [F5]\n";
        updateTitle();
    }

    void cycleSsaoQuality()
    {
        const auto       current = mRenderer->GetSsaoQuality();
        fra::SsaoQuality next    = fra::SsaoQuality::Low;
        switch (current)
        {
            case fra::SsaoQuality::Low:
                next = fra::SsaoQuality::Medium;
                break;
            case fra::SsaoQuality::Medium:
                next = fra::SsaoQuality::High;
                break;
            case fra::SsaoQuality::High:
                next = fra::SsaoQuality::Ultra;
                break;
            case fra::SsaoQuality::Ultra:
                next = fra::SsaoQuality::Low;
                break;
        }
        mRenderer->SetSsaoQuality(next);
        static constexpr const char* kNames[] = { "Low", "Medium", "High",
                                                  "Ultra" };
        std::cout << "SSAO quality: " << kNames[static_cast<int>(next)]
                  << " [F6]\n";
        updateTitle();
    }

    void cycleTaaQuality()
    {
        const auto      current = mRenderer->GetTaaQuality();
        fra::TaaQuality next    = fra::TaaQuality::Low;
        switch (current)
        {
            case fra::TaaQuality::Low:
                next = fra::TaaQuality::Medium;
                break;
            case fra::TaaQuality::Medium:
                next = fra::TaaQuality::High;
                break;
            case fra::TaaQuality::High:
                next = fra::TaaQuality::Ultra;
                break;
            case fra::TaaQuality::Ultra:
                next = fra::TaaQuality::Low;
                break;
        }
        mRenderer->SetTaaQuality(next);
        static constexpr const char* kNames[] = { "Low", "Medium", "High",
                                                  "Ultra" };
        std::cout << "TAA quality: " << kNames[static_cast<int>(next)]
                  << " [F7]\n";
        updateTitle();
    }

    void cycleBloomQuality()
    {
        const auto        current = mRenderer->GetBloomQuality();
        fra::BloomQuality next    = fra::BloomQuality::Low;
        switch (current)
        {
            case fra::BloomQuality::Low:
                next = fra::BloomQuality::Medium;
                break;
            case fra::BloomQuality::Medium:
                next = fra::BloomQuality::High;
                break;
            case fra::BloomQuality::High:
                next = fra::BloomQuality::Ultra;
                break;
            case fra::BloomQuality::Ultra:
                next = fra::BloomQuality::Low;
                break;
        }
        mRenderer->SetBloomQuality(next);
        static constexpr const char* kNames[] = { "Low", "Medium", "High",
                                                  "Ultra" };
        std::cout << "Bloom quality: " << kNames[static_cast<int>(next)]
                  << " [F8]\n";
        updateTitle();
    }

    void updateTitle()
    {
        static constexpr const char* kQuality[] = { "L", "M", "H", "U" };
        static constexpr const char* kShadow[]  = {
            "all", "dir", "warmPt", "coolPt", "spots",
        };
        auto qName = [](int index) {
            return (index >= 0 && index <= 3) ? kQuality[index] : "?";
        };
        const char* shadowName =
            (mShadowCasterMode >= 0 && mShadowCasterMode <= 4)
                ? kShadow[mShadowCasterMode]
                : "?";
        mFreyaOptions->title =
            std::string("Industrial Pipe Lamp | Shd ") +
            qName(static_cast<int>(mRenderer->GetShadowQuality())) +
            " [F5] SSAO " +
            qName(static_cast<int>(mRenderer->GetSsaoQuality())) +
            " [F6] TAA " + qName(static_cast<int>(mRenderer->GetTaaQuality())) +
            " [F7] Blm " +
            qName(static_cast<int>(mRenderer->GetBloomQuality())) + " [F8] | " +
            shadowName + " [0-4]";
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

    std::uint32_t mGroundMesh {};
    std::uint32_t mGroundMaterial {};

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
    glm::mat4                   mModelMatrix[kInstanceCount] {};
    float                       mCurrentTime {};
    std::vector<AnimatedLight>  mAnimatedLights;

    std::unordered_set<std::uint32_t> mKeysHeld;
    bool                              mLookHeld  = false;
    glm::vec3                         mCameraPos = { 0.0f, 4.0f, 18.0f };
    float                             mYaw       = -90.0f;
    float                             mPitch     = -12.0f;

    std::uint32_t              mDirectionalIndex = 0;
    std::uint32_t              mWarmPointIndex   = 0;
    std::uint32_t              mCoolPointIndex   = 0;
    std::vector<std::uint32_t> mSpotIndices;
    int                        mShadowCasterMode = 0;
};

int main(int argc, const char** argv)
{
    const auto app =
        skr::ApplicationBuilder()
            .WithExtension<fra::FreyaExtension>([](fra::FreyaExtension freya) {
                freya.WithOptions([](fra::FreyaOptionsBuilder& freyaOptions) {
                    freyaOptions
                        .SetTitle("Industrial Pipe Lamp — Deferred [RMB+WASD]")
                        .SetWidth(1920)
                        .SetHeight(1080)
                        .SetVSync(false)
                        .SetSampleCount(8)
                        .WithReverseZ()
                        .SetIblIntensity(0.12f)
                        .SetShadowQuality(fra::ShadowQuality::High)
                        .SetShadowBias(0.002f)
                        .SetShadowLightSize(0.035f)
                        .SetShadowMaxSoftness(8.0f)
                        .SetShadowMinVisibility(0.0f)
                        .SetFullscreen(false);
                });
            })
            .Build<MainApp>();

    app->Run();

    return 0;
}
