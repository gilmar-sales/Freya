#include <Freya/Freya.hpp>

#include <FreyaExamples/CullAabbDebugDraw.hpp>
#include <FreyaExamples/DebugOverlay.hpp>
#include <FreyaExamples/ExampleLogging.hpp>
#include <FreyaExamples/FlyCam.hpp>
#include <FreyaExamples/GroundMesh.hpp>

#include <cmath>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

class MainApp final : public fra::AbstractApplication
{
  public:
    explicit MainApp(const fra::Ref<fra::ServiceProvider>& serviceProvider) :
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
        mOverlay.SetCullDumpExampleName("IndustrialPipeLamp");

        mEventManager->Subscribe<fra::KeyReleasedEvent>(
            [this](const fra::KeyReleasedEvent& event) {
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
        mBulbMesh =
            mLampModel.size() > 1 ? mLampModel[1].mesh : fra::MeshHandle {};
        if (!mBulbMesh.IsValid())
        {
            std::cerr << "Lamp GLB has " << mLampModel.size()
                      << " submesh(es); expected body/bulb/switch — "
                         "bulb Blend material will not apply\n";
        }

        mGroundMesh = FreyaExamples::CreateGroundPlane(
            *mMeshPool, 80.0f, glm::vec3(0.72f, 0.72f, 0.76f));
        mGroundMaterial = mMaterialPool->Create({});

        // Fill lights stay dim so local casters dominate when diagnosed.
        // castShadows is toggled one-at-a-time (spots all share mode 4).
        {
            auto key = fra::MakeDirectionalLight(glm::vec3(-0.4f, -1.0f, -0.3f),
                                                 glm::vec3(1.0f, 0.96f, 0.9f),
                                                 0.35f);
            key.castShadows    = false;
            mDirectionalHandle = mLightService->AddLight(key);
        }

        mAnimatedLights.clear();
        mSpotHandles.clear();

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
            warm.handle       = mLightService->AddLight(point);
            mWarmPointHandle  = warm.handle;
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
            cool.handle       = mLightService->AddLight(point);
            mCoolPointHandle  = cool.handle;
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
            spotAnim.handle  = mLightService->AddLight(spot);
            mSpotHandles.push_back(spotAnim.handle);
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
        mBulbSpotHandles.clear();
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
            mBulbSpotHandles.push_back(mLightService->AddLight(spot));
        }
        updateBulbSpots();

        rebuildScene(mScene, true);

        std::cout
            << "Controls: RMB look | WASD move | Space/Q up | Ctrl/E down | "
               "Esc release mouse (per window)\n"
            << "Shadow test: 0=all  1=directional  2=warm point  "
               "3=cool point  4=all spots | F3 light gizmos | "
               "F10 secondary window\n"
            << "TAA check: lamp 0 orbits — ghost trail => bad velocity\n"
            << "ImGui: Freya Debug panel (timing / quality / deferred "
               "views / GPU Cull > Show cull AABBs)\n";

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
                mLightService->UpdateLightPosition(animated.handle, position);
                continue;
            }

            const auto* current = mLightService->GetLight(animated.handle);
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
            mLightService->UpdateLight(animated.handle, spot);
        }

        // Orbit lamp 0 so TAA object motion can be validated (lamp 1 + ground
        // stay static). Ghosting on the moving lamp with TAA on, gone with
        // TAA off (ImGui quality Off), points at bad velocity history.
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
        syncTransforms(mScene, true);
        mScene.Upload(*mRenderer);

        if (mOverlay.ShowCullAabbs())
            drawCullAabbs();

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

        mSecondaryCam.Update(window->GetDeltaTime());

        renderer->BeginFrame();
        mSecondaryCam.Apply(*renderer);
        syncTransforms(mSecondaryScene, false);
        mSecondaryScene.Upload(*renderer);
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

        auto lights = GetWindowServices(*mSecondaryWindow)
                          ->GetService<fra::LightService>();
        lights->ClearLights();
        lights->AddLight(fra::MakeDirectionalLight(
            glm::vec3(-0.2f, -1.0f, -0.15f), glm::vec3(1.0f, 0.96f, 0.9f),
            2.5f));
        rebuildScene(mSecondaryScene, false);

        std::cout << "Secondary window opened (shared meshes; RMB+WASD)\n";
    }

    void rebuildScene(fra::Scene& scene, const bool bothLamps)
    {
        scene.Clear();
        const std::uint32_t lampCount = bothLamps ? 2u : 1u;
        for (const auto& part : mLampModel)
        {
            const bool isBulb = part.mesh == mBulbMesh;
            for (std::uint32_t i = 0; i < lampCount; ++i)
            {
                fra::Scene::Instance inst {};
                inst.transform =
                    fra::SceneTransform::FromMatrix(mModelMatrix[i]);
                inst.mesh        = part.mesh;
                inst.material    = isBulb ? mBulbMaterial : mSofaMaterial;
                inst.entityId    = i + 1;
                inst.castShadows = !isBulb;
                inst.mobility    = fra::Mobility::Dynamic;
                scene.Add(inst);
            }
        }
        fra::Scene::Instance ground {};
        ground.transform   = fra::SceneTransform::FromMatrix(mModelMatrix[2]);
        ground.mesh        = mGroundMesh;
        ground.material    = mGroundMaterial;
        ground.entityId    = 0;
        ground.castShadows = false;
        ground.mobility    = fra::Mobility::Static;
        scene.Add(ground);
    }

    void syncTransforms(fra::Scene& scene, const bool bothLamps)
    {
        // Stable ids 0..n-1 from rebuildScene (Clear then Add in order).
        // Ground is Static — skip SetTransform so Upload can patch/no-op.
        const std::uint32_t    lampCount = bothLamps ? 2u : 1u;
        fra::Scene::InstanceId id        = 0;
        for (std::size_t part = 0; part < mLampModel.size(); ++part)
        {
            for (std::uint32_t i = 0; i < lampCount; ++i)
                scene.SetTransform(id++, mModelMatrix[i]);
        }
    }

    /**
     * @brief Draws the exact world-space AABB the GPU cull compute shader
     * (Shaders/GpuDriven/CullFrustum.comp) tests each instance against:
     * MeshPool's registered mesh-local aabbMin/aabbMax transformed by the
     * instance's model matrix. Bulb submeshes are highlighted distinctly
     * from the lamp body. Toggle via the "Show cull AABBs" checkbox in the
     * ImGui "Freya Debug" panel (GPU Cull section).
     */
    void drawCullAabbs()
    {
        auto& dd = mRenderer->GetDebugDraw();
        for (const auto& part : mLampModel)
        {
            const bool      isBulb = part.mesh == mBulbMesh;
            const glm::vec4 color  = isBulb ? glm::vec4(1.0f, 0.8f, 0.2f, 0.9f)
                                            : glm::vec4(0.2f, 0.9f, 1.0f, 0.6f);
            for (std::uint32_t i = 0; i < 2; ++i)
                FreyaExamples::DrawCullAabb(
                    dd, *mMeshPool, part.mesh, mModelMatrix[i], color);
        }
        FreyaExamples::DrawCullAabb(
            dd, *mMeshPool, mGroundMesh, mModelMatrix[2],
            glm::vec4(0.6f, 0.9f, 0.4f, 0.5f));
    }

    static constexpr std::size_t kInstanceCount = 3;

    void updateBulbSpots()
    {
        // Local bulb center after Assimp KEEP_HIERARCHY pre-transform
        // (node translation + mesh AABB center of Roundcube.005).
        constexpr glm::vec3 kBulbLocal { 0.0f, 0.360f, 0.062f };
        // Cage opens slightly toward +Z; bias aim down/out of the fixture.
        constexpr glm::vec3 kAimLocal { 0.0f, -0.85f, 0.45f };

        for (std::size_t i = 0; i < mBulbSpotHandles.size() && i < 2; ++i)
        {
            const auto* current = mLightService->GetLight(mBulbSpotHandles[i]);
            if (current == nullptr)
                continue;

            const glm::mat4& model = mModelMatrix[i];
            fra::Light       spot  = *current;
            spot.position = glm::vec3(model * glm::vec4(kBulbLocal, 1.0f));
            const glm::vec3 aim = glm::mat3(model) * kAimLocal;
            if (glm::dot(aim, aim) > 1e-8f)
                spot.direction = glm::normalize(aim);
            mLightService->UpdateLight(mBulbSpotHandles[i], spot);
        }
    }

    void setLightCastShadows(fra::LightHandle handle, bool enabled)
    {
        const auto* current = mLightService->GetLight(handle);
        if (current == nullptr)
        {
            return;
        }

        fra::Light light  = *current;
        light.castShadows = enabled;
        mLightService->UpdateLight(handle, light);
    }

    void setShadowCasterMode(int mode)
    {
        mShadowCasterMode = mode;

        const bool all = mode == 0;
        setLightCastShadows(mDirectionalHandle, all || mode == 1);
        setLightCastShadows(mWarmPointHandle, all || mode == 2);
        setLightCastShadows(mCoolPointHandle, all || mode == 3);
        // Orbiting spots are diagnostic — only mode 4 (reserve shadow slots
        // for the per-bulb spots in the default "all" mode).
        for (const auto spotHandle : mSpotHandles)
            setLightCastShadows(spotHandle, mode == 4);
        // Bulb spots sit inside the housing; they must not own shadow slots
        // in the default mode or the floor umbra is a huge near-field blob.
        for (const auto spotHandle : mBulbSpotHandles)
            setLightCastShadows(spotHandle, false);

        static constexpr const char* kNames[] = {
            "all", "directional", "warm point", "cool point", "all spots",
        };
        const char* name = (mode >= 0 && mode <= 4) ? kNames[mode] : "unknown";
        std::cout << "Shadow caster: " << name << " [" << mode << "]\n";
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
            std::string("Industrial Pipe Lamp | casters ") + shadowName +
            " [0-4]" + (mShowLightGizmos ? " | gizmos" : "") + " | ImGui debug";
    }

    void drawLightGizmos()
    {
        auto&      dd    = mRenderer->GetDebugDraw();
        const auto count = mLightService->GetLightCount();
        for (std::uint32_t i = 0; i < count; ++i)
        {
            const auto* light = mLightService->GetLight(fra::LightHandle { i });
            if (light == nullptr)
            {
                continue;
            }

            const auto peak = std::max(
                { light->color.r, light->color.g, light->color.b, 0.2f });
            glm::vec4 color(light->color / peak,
                            light->castShadows ? 1.0f : 0.4f);

            switch (light->type)
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
        fra::LightHandle  handle {};
        AnimatedLightKind kind         = AnimatedLightKind::Point;
        float             speed        = 1.0f;
        float             radiusOffset = 0.0f;
        float             phaseOffset  = 0.0f;
    };

    std::vector<fra::ModelSubmesh>    mLampModel;
    std::optional<fra::TextureHandle> mSofaAlbedo {};
    std::optional<fra::TextureHandle> mSofaNormal {};
    std::optional<fra::TextureHandle> mSofaRoughness {};
    std::optional<fra::TextureHandle> mSofaEmissive {};
    std::optional<fra::TextureHandle> mSofaMetalness {};
    fra::MaterialHandle               mSofaMaterial {};
    fra::MaterialHandle               mBulbMaterial {};
    fra::MeshHandle                   mBulbMesh {};

    fra::MeshHandle     mGroundMesh {};
    fra::MaterialHandle mGroundMaterial {};
    fra::Scene          mScene;
    fra::Scene          mSecondaryScene;

    fra::Ref<fra::MaterialPool> mMaterialPool;
    fra::Ref<fra::TexturePool>  mTexturePool;
    fra::Ref<fra::MeshPool>     mMeshPool;
    fra::Ref<fra::LightService> mLightService;
    fra::Ref<fra::FreyaOptions> mFreyaOptions;
    fra::Ref<fra::Window>       mSecondaryWindow;
    FreyaExamples::FlyCam       mMainCam;
    FreyaExamples::FlyCam       mSecondaryCam;
    FreyaExamples::DebugOverlay mOverlay;
    glm::mat4                   mModelMatrix[kInstanceCount] {};
    float                       mCurrentTime {};
    std::vector<AnimatedLight>  mAnimatedLights;

    fra::LightHandle              mDirectionalHandle {};
    fra::LightHandle              mWarmPointHandle {};
    fra::LightHandle              mCoolPointHandle {};
    std::vector<fra::LightHandle> mSpotHandles;
    std::vector<fra::LightHandle> mBulbSpotHandles;
    int                           mShadowCasterMode = 0;
    bool                          mShowLightGizmos  = true;
};

int main(int, const char**)
{
    return fra::RunApp<MainApp>(
        [](fra::FreyaOptionsBuilder& freyaOptions) {
            freyaOptions.SetTitle("Industrial Pipe Lamp — Deferred [RMB+WASD]")
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
        },
        [](skr::LoggingExtension& l) {
            FreyaExamples::ConfigureLogging(l, "IndustrialPipeLamp.log");
        });
}
