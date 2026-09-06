#include <Freya/Freya.hpp>

#include <FreyaExamples/DebugOverlay.hpp>
#include <FreyaExamples/FlyCam.hpp>
#include <FreyaExamples/GroundMesh.hpp>
#include <FreyaExamples/QualityCycle.hpp>

#include <cmath>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

class MainApp final : public fra::AbstractApplication
{
  public:
    explicit MainApp(const skr::Arc<skr::ServiceProvider>& serviceProvider) :
        AbstractApplication(serviceProvider)
    {
        auto windowServices = GetMainServiceProvider();
        mMeshPool           = serviceProvider->GetService<fra::MeshPool>();
        mTexturePool        = serviceProvider->GetService<fra::TexturePool>();
        mMaterialPool       = serviceProvider->GetService<fra::MaterialPool>();
        mLightService       = windowServices->GetService<fra::LightService>();
        mFreyaOptions       = windowServices->GetService<fra::FreyaOptions>();
    }

    void StartUp() override
    {
        mMainCam.window     = mWindow;
        mMainCam.blockMouse = [this] { return mOverlay.WantsCaptureMouse(); };
        mMainCam.BindInput(*mEventManager);

        if (mPlatform)
            mOverlay.Init(*mRenderer, *mWindow, *mPlatform);

        mEventManager->Subscribe<fra::KeyReleasedEvent>(
            [this](const fra::KeyReleasedEvent& event) {
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
                if (event.key == fra::KeyCode::F3)
                {
                    mShowLightGizmos = !mShowLightGizmos;
                    mRenderer->SetDebugDrawEnabled(mShowLightGizmos);
                    std::cout
                        << "Light gizmos: " << (mShowLightGizmos ? "on" : "off")
                        << '\n';
                    updateTitle();
                    return;
                }
                if (event.key == fra::KeyCode::F9)
                {
                    const bool next = !mRenderer->GetShadowDebug();
                    mRenderer->SetShadowDebug(next);
                    std::cout
                        << "Shadow debug: " << (next ? "on" : "off") << '\n';
                    updateTitle();
                    return;
                }
                if (event.key == fra::KeyCode::F10)
                {
                    toggleSecondaryWindow();
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

        updateTitle();
        mRenderer->SetDebugDrawEnabled(mShowLightGizmos);
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
            { .albedo             = mSofaAlbedo,
              .normal             = mSofaNormal,
              .roughness          = mSofaRoughness,
              .emissive           = mSofaEmissive,
              .metalness          = mSofaMetalness,
              .clearcoat          = 0.85f,
              .clearcoatRoughness = 0.08f });

        // Warm amber glass — transmission + IOR drive screen-space refraction.
        mBulbMaterial = mMaterialPool->Create({
            .emissive        = mSofaEmissive,
            .albedoFactor    = { 0.95f, 0.78f, 0.45f, 0.12f },
            .roughnessFactor = 0.04f,
            .metalnessFactor = 0.0f,
            .emissiveFactor  = { 0.40f, 0.24f, 0.08f },
            .alphaMode       = fra::AlphaMode::Blend,
            .transmission    = 0.92f,
            .ior             = 1.5f,
        });

        mLampModel = mMeshPool->CreateModelFromFile(
            "./Resources/Models/industrial_pipe_lamp.glb");
        // GLB node order with KEEP_HIERARCHY: body (0), bulb (1), switch (2).
        mBulbMeshId = mLampModel.size() > 1 ? mLampModel[1].meshId
                                            : std::uint32_t { ~0u };
        if (mBulbMeshId == ~0u)
        {
            std::cerr << "Lamp GLB has " << mLampModel.size()
                      << " submesh(es); expected body/bulb/switch — "
                         "bulb Blend material will not apply\n";
        }

        mGroundMesh = FreyaExamples::CreateGroundPlane(
            *mMeshPool, 80.0f, glm::vec3(0.72f, 0.72f, 0.76f));
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
            mMeshPool->CreateModelFromFile("./Resources/Models/SpaceShip.fbx");

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
            warm.radiusOffset = 1.5f;
            warm.kind         = AnimatedLightKind::Point;
            auto point        = fra::MakePointLight(
                glm::vec3(10.0f, 12.0f, 0.0f),
                glm::vec3(1.0f, 0.45f, 0.3f),
                48.0f,
                8.0f);
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
            cool.radiusOffset = 2.0f;
            cool.kind         = AnimatedLightKind::Point;
            auto point        = fra::MakePointLight(
                glm::vec3(-10.0f, 12.0f, 0.0f),
                glm::vec3(0.3f, 0.5f, 1.0f),
                48.0f,
                8.0f);
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
            { 0.90f, 4.0f, 1.0f, { 0.95f, 0.95f, 1.00f } },
            { 0.75f, 0.8f, 2.2f, { 1.00f, 0.85f, 0.55f } },
            { 1.05f, 2.6f, 0.5f, { 0.55f, 0.85f, 1.00f } },
            { 0.85f, 5.2f, 2.8f, { 1.00f, 0.55f, 0.70f } },
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
                50.0f,
                glm::radians(14.0f),
                glm::radians(24.0f),
                7.0f);
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

        // One warm spot per lamp bulb (follow mModelMatrix each frame).
        mBulbSpotIndices.clear();
        for (std::uint32_t i = 0; i < 2; ++i)
        {
            auto spot = fra::MakeSpotLight(
                glm::vec3(0.0f),
                glm::vec3(0.0f, -1.0f, 0.0f),
                glm::vec3(1.0f, 0.82f, 0.55f),
                28.0f,
                glm::radians(28.0f),
                glm::radians(48.0f),
                22.0f);
            spot.castShadows = false;
            mBulbSpotIndices.push_back(
                static_cast<std::uint32_t>(mLightService->AddLight(spot)));
        }
        updateBulbSpots();

        std::cout
            << "Controls: RMB look | WASD move | Space/Q up | Ctrl/E down | "
               "Esc release mouse (per window)\n"
            << "Shadow test: 0=all  1=directional  2=warm point  "
               "3=cool point  4=all spots | F3 light gizmos | "
               "F9 shadow factor | F10 secondary window\n"
            << "TAA check: lamp 0 orbits — ghost trail => bad velocity; "
               "F5–F8 cycle quality (Low→Med→High→Ultra→Off)\n"
            << "ImGui: Freya Debug panel (timing / quality / SSAO views)\n";

        // All casters on by default (same as key 0).
        setShadowCasterMode(0);
    }

    void Update() override
    {
        mOverlay.MarkUpdateStart();
        mOverlay.BeginFrame();

        const float dt = mWindow->GetDeltaTime();
        mCurrentTime += dt;
        mMainCam.Update(dt);

        for (auto& animated : mAnimatedLights)
        {
            const float offset = animated.phaseOffset;
            const float radius = 10.0f + animated.radiusOffset;

            const float x =
                radius * std::cos(animated.speed * mCurrentTime + offset);
            const float z =
                radius *
                std::sin(animated.speed * mCurrentTime + offset * 1.3f);
            const float y =
                12.0f +
                1.5f * std::sin(animated.speed * 0.7f * mCurrentTime + offset);
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

            fra::Light spot          = *current;
            spot.position            = position;
            const glm::vec3 toTarget = glm::vec3(0.0f, -6.0f, 0.0f) - position;
            if (glm::length(toTarget) > 1e-4f)
            {
                spot.direction = glm::normalize(toTarget);
            }
            mLightService->UpdateLight(animated.index, spot);
        }

        // Orbit lamp 0 so TAA object motion can be validated (lamp 1 + ground
        // stay static). Ghosting on the moving lamp with TAA on, gone with
        // TAA off (F7 quality / disable), points at bad velocity history.
        {
            constexpr float kOrbitRadius = 4.0f;
            constexpr float kOrbitSpeed  = 0.9f;
            const float     angle        = kOrbitSpeed * mCurrentTime;
            const glm::vec3 pos(kOrbitRadius * std::cos(angle),
                                -6.0f,
                                kOrbitRadius * std::sin(angle));
            mModelMatrix[0] = glm::scale(glm::translate(glm::mat4(1.0f), pos),
                                         glm::vec3(28.0f));
        }

        updateBulbSpots();

        mRenderer->BeginFrame();
        if (mShowLightGizmos)
        {
            drawLightGizmos();
        }

        mMainCam.Apply(*mRenderer);
        mRenderer->UploadSceneInstances(buildSceneInstances(true));

        const float cpuFrameMs  = mWindow->GetDeltaTime() * 1000.f;
        const float cpuUpdateMs = mOverlay.ElapsedUpdateMs();
        mOverlay.Draw(*mRenderer, *mFreyaOptions, cpuFrameMs, cpuUpdateMs);
        mOverlay.EndFrame(*mRenderer);
    }

    void UpdateSecondaryWindow(const skr::Arc<fra::Window>& window) override
    {
        if (!window || !window->IsRunning())
            return;

        auto renderer = GetRenderer(*window);
        auto lights =
            GetWindowServices(*window)->GetService<fra::LightService>();

        lights->ClearLights();
        lights->AddLight(fra::MakeDirectionalLight(
            glm::vec3(-0.2f, -1.0f, -0.15f), glm::vec3(1.0f, 0.96f, 0.9f),
            2.5f));

        mSecondaryCam.Update(window->GetDeltaTime());

        renderer->BeginFrame();
        mSecondaryCam.Apply(*renderer);
        renderer->UploadSceneInstances(buildSceneInstances(false));
        renderer->EndFrame();
    }

  private:
    void toggleSecondaryWindow()
    {
        if (mSecondaryWindow)
        {
            if (mSecondaryWindow->IsRunning())
                mSecondaryWindow->Close();
            mSecondaryWindow = nullptr;
            mSecondaryCam    = {};
            std::cout << "Secondary window closed\n";
            return;
        }

        mSecondaryWindow = CreateWindow([](fra::FreyaOptionsBuilder& o) {
            o.SetTitle("Industrial Pipe Lamp — Secondary [RMB+WASD | F10]")
                .SetWidth(1280)
                .SetHeight(720)
                .SetFullscreen(false)
                .SetVSync(false);
        });

        mSecondaryCam        = mMainCam;
        mSecondaryCam.window = mSecondaryWindow;
        mSecondaryCam.keysHeld.clear();
        mSecondaryCam.lookHeld = false;

        const auto events = GetWindowServices(*mSecondaryWindow)
                                ->GetService<fra::EventManager>();
        mSecondaryCam.BindInput(*events);
        events->Subscribe<fra::KeyReleasedEvent>(
            [this](const fra::KeyReleasedEvent& event) {
                if (event.key == fra::KeyCode::F10)
                    toggleSecondaryWindow();
            });

        std::cout << "Secondary window opened (shared meshes; RMB+WASD)\n";
    }

    [[nodiscard]] std::vector<fra::SceneInstanceUpload> buildSceneInstances(
        const bool bothLamps) const
    {
        const std::uint32_t                   lampCount = bothLamps ? 2u : 1u;
        std::vector<fra::SceneInstanceUpload> instances;
        instances.reserve(mLampModel.size() * lampCount + 1);
        for (const auto& part : mLampModel)
        {
            const bool isBulb = part.meshId == mBulbMeshId;
            for (std::uint32_t i = 0; i < lampCount; ++i)
            {
                instances.push_back(fra::SceneInstanceUpload {
                    .model       = mModelMatrix[i],
                    .meshId      = part.meshId,
                    .materialId  = isBulb ? mBulbMaterial : mSofaMaterial,
                    .entityId    = i + 1,
                    .castShadows = !isBulb,
                });
            }
        }
        instances.push_back(fra::SceneInstanceUpload {
            .model       = mModelMatrix[2],
            .meshId      = mGroundMesh,
            .materialId  = mGroundMaterial,
            .entityId    = 0,
            .castShadows = false,
        });
        return instances;
    }

    static constexpr std::size_t kInstanceCount = 3;

    void updateBulbSpots()
    {
        // Local bulb center after Assimp KEEP_HIERARCHY pre-transform
        // (node translation + mesh AABB center of Roundcube.005).
        constexpr glm::vec3 kBulbLocal { 0.0f, 0.360f, 0.062f };
        // Cage opens slightly toward +Z; bias aim down/out of the fixture.
        constexpr glm::vec3 kAimLocal { 0.0f, -0.85f, 0.45f };

        for (std::size_t i = 0; i < mBulbSpotIndices.size() && i < 2; ++i)
        {
            const auto* current = mLightService->GetLight(mBulbSpotIndices[i]);
            if (current == nullptr)
                continue;

            const glm::mat4& model = mModelMatrix[i];
            fra::Light       spot  = *current;
            spot.position = glm::vec3(model * glm::vec4(kBulbLocal, 1.0f));
            const glm::vec3 aim = glm::mat3(model) * kAimLocal;
            if (glm::dot(aim, aim) > 1e-8f)
                spot.direction = glm::normalize(aim);
            mLightService->UpdateLight(mBulbSpotIndices[i], spot);
        }
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
        // Orbiting spots are diagnostic — only mode 4 (reserve shadow slots
        // for the per-bulb spots in the default "all" mode).
        for (const auto spotIndex : mSpotIndices)
            setLightCastShadows(spotIndex, mode == 4);
        // Bulb spots sit inside the housing; they must not own shadow slots
        // in the default mode or the floor umbra is a huge near-field blob.
        for (const auto spotIndex : mBulbSpotIndices)
            setLightCastShadows(spotIndex, false);

        static constexpr const char* kNames[] = {
            "all", "directional", "warm point", "cool point", "all spots",
        };
        const char* name = (mode >= 0 && mode <= 4) ? kNames[mode] : "unknown";
        std::cout << "Shadow caster: " << name << " [" << mode << "]\n";
        updateTitle();
    }

    void cycleShadowQuality()
    {
        const auto next =
            FreyaExamples::CycleQuality(mRenderer->GetShadowQuality());
        mRenderer->SetShadowQuality(next);
        std::cout << "Shadow quality: " << FreyaExamples::QualityName(next)
                  << " [F5]\n";
        updateTitle();
    }

    void cycleSsaoQuality()
    {
        const auto next =
            FreyaExamples::CycleQuality(mRenderer->GetSsaoQuality());
        mRenderer->SetSsaoQuality(next);
        std::cout << "SSAO quality: " << FreyaExamples::QualityName(next)
                  << " [F6]\n";
        updateTitle();
    }

    void cycleTaaQuality()
    {
        const auto next =
            FreyaExamples::CycleQuality(mRenderer->GetTaaQuality());
        mRenderer->SetTaaQuality(next);
        std::cout << "TAA quality: " << FreyaExamples::QualityName(next)
                  << " [F7]\n";
        updateTitle();
    }

    void cycleBloomQuality()
    {
        const auto next =
            FreyaExamples::CycleQuality(mRenderer->GetBloomQuality());
        mRenderer->SetBloomQuality(next);
        std::cout << "Bloom quality: " << FreyaExamples::QualityName(next)
                  << " [F8]\n";
        updateTitle();
    }

    void updateTitle()
    {
        static constexpr const char* kShadow[] = {
            "all", "dir", "warmPt", "coolPt", "spots",
        };
        const char* shadowName =
            (mShadowCasterMode >= 0 && mShadowCasterMode <= 4)
                ? kShadow[mShadowCasterMode]
                : "?";
        mFreyaOptions->title =
            std::string("Industrial Pipe Lamp | Shd ") +
            FreyaExamples::QualityShortName(
                static_cast<int>(mRenderer->GetShadowQuality())) +
            " [F5] SSAO " +
            FreyaExamples::QualityShortName(
                static_cast<int>(mRenderer->GetSsaoQuality())) +
            " [F6] TAA " +
            FreyaExamples::QualityShortName(
                static_cast<int>(mRenderer->GetTaaQuality())) +
            " [F7] Blm " +
            FreyaExamples::QualityShortName(
                static_cast<int>(mRenderer->GetBloomQuality())) +
            " [F8] | " + (mRenderer->GetShadowDebug() ? "shdDBG " : "") +
            (mShowLightGizmos ? "gizmo " : "") + shadowName + " [0-4]";
    }

    void drawLightGizmos()
    {
        auto&      dd    = mRenderer->GetDebugDraw();
        const auto count = mLightService->GetLightCount();
        for (std::uint32_t i = 0; i < count; ++i)
        {
            const auto* light = mLightService->GetLight(i);
            if (light == nullptr)
            {
                continue;
            }

            const auto peak = std::max(
                { light->color.r, light->color.g, light->color.b, 0.2f });
            glm::vec4 color(light->color / peak,
                            light->castShadows ? 1.0f : 0.4f);

            const auto type = static_cast<fra::LightType>(
                static_cast<std::uint32_t>(light->type + 0.5f));
            switch (type)
            {
                case fra::LightType::Point:
                    dd.Sphere(light->position, 0.7f, color, 16);
                    dd.Circle(
                        light->position, { 0.0f, 1.0f, 0.0f }, light->radius,
                        { color.r, color.g, color.b, color.a * 0.35f }, 32);
                    break;
                case fra::LightType::Spot: {
                    const auto outer =
                        std::clamp(light->outerCutoff, -1.0f, 1.0f);
                    const auto half = std::acos(outer);
                    const auto len  = std::min(light->radius * 0.45f, 18.0f);
                    dd.Cone(light->position, light->direction, len, half,
                            color);
                    dd.Sphere(light->position, 0.35f, color, 10);
                    break;
                }
                case fra::LightType::Directional: {
                    constexpr glm::vec3 kAnchor(0.0f, 42.0f, 0.0f);
                    const auto dir = glm::length(light->direction) > 1e-4f
                                         ? glm::normalize(light->direction)
                                         : glm::vec3(0.0f, -1.0f, 0.0f);
                    dd.Arrow(kAnchor - dir * 18.0f, kAnchor + dir * 8.0f,
                             color);
                    break;
                }
                case fra::LightType::Area:
                    dd.Rect(light->position, light->direction, light->tangent,
                            light->outerCutoff, light->halfHeight, color);
                    break;
            }
        }
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

    std::vector<fra::ModelSubmesh> mLampModel;
    std::optional<std::uint32_t>   mSofaAlbedo {};
    std::optional<std::uint32_t>   mSofaNormal {};
    std::optional<std::uint32_t>   mSofaRoughness {};
    std::optional<std::uint32_t>   mSofaEmissive {};
    std::optional<std::uint32_t>   mSofaMetalness {};
    std::uint32_t                  mSofaMaterial {};
    std::uint32_t                  mBulbMaterial {};
    std::uint32_t                  mBulbMeshId { ~0u };

    std::uint32_t mGroundMesh {};
    std::uint32_t mGroundMaterial {};

    std::vector<fra::ModelSubmesh> mSpaceShipModel;
    std::optional<std::uint32_t>   mSpaceShipAlbedo {};
    std::optional<std::uint32_t>   mSpaceShipNormal {};
    std::optional<std::uint32_t>   mSpaceShipRoughness {};
    std::uint32_t                  mSpaceShipMaterial {};

    skr::Arc<fra::MaterialPool> mMaterialPool;
    skr::Arc<fra::TexturePool>  mTexturePool;
    skr::Arc<fra::MeshPool>     mMeshPool;
    skr::Arc<fra::LightService> mLightService;
    skr::Arc<fra::FreyaOptions> mFreyaOptions;
    skr::Arc<fra::Window>       mSecondaryWindow;
    FreyaExamples::FlyCam       mMainCam;
    FreyaExamples::FlyCam       mSecondaryCam;
    FreyaExamples::DebugOverlay mOverlay;
    glm::mat4                   mModelMatrix[kInstanceCount] {};
    float                       mCurrentTime {};
    std::vector<AnimatedLight>  mAnimatedLights;

    std::uint32_t              mDirectionalIndex = 0;
    std::uint32_t              mWarmPointIndex   = 0;
    std::uint32_t              mCoolPointIndex   = 0;
    std::vector<std::uint32_t> mSpotIndices;
    std::vector<std::uint32_t> mBulbSpotIndices;
    int                        mShadowCasterMode = 0;
    bool                       mShowLightGizmos  = true;
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
