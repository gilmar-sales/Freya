#include <Freya/Freya.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>

namespace
{
    // Matches Shaders/Cell/cell.frag push_constant layout.
    struct CellPushConstants
    {
        float     bands           = 4.0f;
        float     edgeDepthScale  = 80.0f;
        float     edgeNormalScale = 2.0f;
        float     strength        = 1.0f;
        glm::vec4 edgeColor { 0.02f, 0.02f, 0.04f, 1.0f };
        float     reverseZ   = 0.0f;
        float     shadowLift = 0.22f;
        float     edgeWidth  = 1.0f;
    };

    struct OutlinePush
    {
        float     edgeDepthScale  = 80.0f;
        float     edgeNormalScale = 2.0f;
        float     strength        = 1.0f;
        float     reverseZ        = 0.0f;
        glm::vec4 edgeColor { 0.02f, 0.02f, 0.04f, 1.0f };
        float     edgeWidth = 1.0f;
        float     _pad0 = 0.f, _pad1 = 0.f, _pad2 = 0.f;
    };

    struct GradePush
    {
        float     contrast   = 1.05f;
        float     saturation = 1.15f;
        float     exposure   = 0.0f;
        float     vignette   = 0.35f;
        glm::vec4 lift { 0.0f };
        glm::vec4 gain { 1.0f, 1.0f, 1.0f, 1.0f };
    };

    struct UnderwaterPush
    {
        float     time         = 0.0f;
        float     strength     = 1.0f;
        float     tintStrength = 0.55f;
        float     fogDensity   = 1.8f;
        glm::vec4 tintColor { 0.15f, 0.45f, 0.55f, 1.0f };
        float     reverseZ = 0.0f;
        float     maxDepth = 0.85f;
        float     _pad0 = 0.f, _pad1 = 0.f;
    };

    struct HeatPush
    {
        float time     = 0.0f;
        float strength = 1.0f;
        float speed    = 1.2f;
        float reverseZ = 0.0f;
    };

    struct GlowPush
    {
        float     intensity = 2.2f;
        float     radius    = 8.0f;
        float     fill      = 0.25f;
        float     reverseZ  = 0.0f;
        glm::vec4 color { 1.0f, 0.85f, 0.25f, 1.0f }; // item gold
    };

    // Mu Online upgrade glow (+0 … +13). Matches Shaders/Post/mu_item_glow.frag.
    struct MuGlowPush
    {
        float time       = 0.0f;
        float level      = 13.0f;
        float intensity  = 1.0f;
        float reverseZ   = 0.0f;
        float radius     = 7.0f;
        float waveSpeed  = 1.0f;
        float _pad0      = 0.0f;
        float _pad1      = 0.0f;
    };

    void ToggleEffect(const skr::Arc<fra::PostProcess>& effect,
                      const char*                       label)
    {
        if (!effect)
            return;
        effect->SetEnabled(!effect->Enabled());
        std::cout << label << ": " << (effect->Enabled() ? "on" : "off")
                  << '\n';
    }
} // namespace

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
        mServices     = serviceProvider;
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

                if (event.key == fra::KeyCode::Escape && mLookHeld)
                {
                    setLookHeld(false);
                    return;
                }
                if (event.key == fra::KeyCode::F4)
                {
                    ToggleEffect(mCellEffect, "Cell");
                    updateTitle();
                    return;
                }
                if (event.key == fra::KeyCode::F5)
                {
                    ToggleEffect(mOutlineEffect, "Outline");
                    updateTitle();
                    return;
                }
                if (event.key == fra::KeyCode::F6)
                {
                    ToggleEffect(mGradeEffect, "ColorGrade");
                    updateTitle();
                    return;
                }
                if (event.key == fra::KeyCode::F7)
                {
                    ToggleEffect(mUnderwaterEffect, "Underwater");
                    updateTitle();
                    return;
                }
                if (event.key == fra::KeyCode::F8)
                {
                    ToggleEffect(mHeatEffect, "HeatHaze");
                    updateTitle();
                    return;
                }
                if (event.key == fra::KeyCode::F9)
                {
                    ToggleEffect(mGlowEffect, "ItemGlow");
                    updateTitle();
                    return;
                }
                if (event.key == fra::KeyCode::F12)
                {
                    ToggleEffect(mMuGlowEffect, "MuGlow");
                    updateTitle();
                    return;
                }
                if (event.key == fra::KeyCode::LeftBracket ||
                    event.key == fra::KeyCode::Minus)
                {
                    mMuGlowLevel = std::max(0, mMuGlowLevel - 1);
                    applyMuGlowLevel();
                    return;
                }
                if (event.key == fra::KeyCode::RightBracket ||
                    event.key == fra::KeyCode::Equals)
                {
                    mMuGlowLevel = std::min(13, mMuGlowLevel + 1);
                    applyMuGlowLevel();
                    return;
                }
                if (event.key == fra::KeyCode::F10)
                {
                    mGroundTriplanar = !mGroundTriplanar;
                    fra::MaterialCreateInfo info =
                        mMaterialPool->GetCreateInfo(mGroundMaterial);
                    info.techniqueId =
                        mGroundTriplanar
                            ? mTriplanarTechnique
                            : fra::MaterialTechniqueRegistry::kDefaultTechnique;
                    mMaterialPool->Update(mGroundMaterial, info);
                    std::cout << "Ground triplanar: "
                              << (mGroundTriplanar ? "on" : "off") << '\n';
                    updateTitle();
                    return;
                }
                if (event.key == fra::KeyCode::F11)
                {
                    mEyesUnlit = !mEyesUnlit;
                    auto setEyeTech = [&](std::uint32_t id,
                                          std::uint32_t cellOrDefault) {
                        auto info = mMaterialPool->GetCreateInfo(id);
                        info.techniqueId =
                            mEyesUnlit ? mUnlitTechnique : cellOrDefault;
                        mMaterialPool->Update(id, info);
                    };
                    setEyeTech(mEyeMaterial, mCellTechnique);
                    setEyeTech(
                        mPbrEyeMaterial,
                        fra::MaterialTechniqueRegistry::kDefaultTechnique);
                    std::cout << "Eyes unlit/emissive: "
                              << (mEyesUnlit ? "on" : "off") << '\n';
                    updateTitle();
                    return;
                }
            });

        mEventManager->Subscribe<fra::MouseButtonPressedEvent>(
            [this](const fra::MouseButtonPressedEvent& event) {
                if (event.button == fra::MouseButton::Right)
                    setLookHeld(true);
            });

        mEventManager->Subscribe<fra::MouseButtonReleasedEvent>(
            [this](const fra::MouseButtonReleasedEvent& event) {
                if (event.button == fra::MouseButton::Right)
                    setLookHeld(false);
            });

        mEventManager->Subscribe<fra::MouseMoveEvent>(
            [this](const fra::MouseMoveEvent& event) {
                if (!mLookHeld)
                    return;
                mYaw += event.deltaX * kMouseSensitivity;
                mPitch -= event.deltaY * kMouseSensitivity;
                mPitch = std::clamp(mPitch, -89.0f, 89.0f);
            });

        mRenderer->ClearProjections();

        auto techniques =
            mServices->GetService<fra::MaterialTechniqueRegistry>();
        mCellTechnique = techniques->Register(
            "CellGBuffer", "Cell/gbuffer_cell.frag.spv");
        mTriplanarTechnique = techniques->Register(
            "Triplanar", "Material/triplanar.frag.spv");
        mUnlitTechnique = techniques->Register(
            "UnlitEmissive", "Material/unlit_emissive.frag.spv");

        auto lighting =
            mServices->GetService<fra::LightingTechniqueRegistry>();
        lighting->SetFragment("Cell/lighting_cell.frag.spv");

        mRenderer->RebuildSwapChain();

        const auto revZ = mFreyaOptions->ReverseZ ? 1.0f : 0.0f;
        auto       insertPost = [&](skr::Arc<fra::PostProcess> effect) {
            if (effect)
                mRenderer->InsertFrameStage("BillboardVfx", effect->MakeStage());
        };

        mCellEffect =
            mServices->GetService<fra::PostProcessBuilder>()
                ->SetName("Cell")
                .SetFragment("Cell/cell.frag.spv")
                .SetInputs({ fra::PostProcessInput::SceneColor,
                             fra::PostProcessInput::Depth,
                             fra::PostProcessInput::Normal })
                .SetPushConstantSize(sizeof(CellPushConstants))
                .Build();
        if (mCellEffect)
        {
            CellPushConstants cell {};
            cell.bands           = 4.0f;
            cell.edgeDepthScale  = 140.0f;
            cell.edgeNormalScale = 1.6f;
            cell.strength        = 1.0f;
            cell.edgeColor       = { 0.05f, 0.08f, 0.04f, 1.0f };
            cell.reverseZ        = revZ;
            cell.shadowLift      = 0.1f;
            cell.edgeWidth       = 2.0f;
            mCellEffect->SetPushConstants(cell);
            // LightingTechniqueRegistry supplies cel bands; F4 post is
            // optional re-band + edges on already-lit HDR.
            mCellEffect->SetEnabled(false);
            insertPost(mCellEffect);
        }

        mOutlineEffect =
            mServices->GetService<fra::PostProcessBuilder>()
                ->SetName("Outline")
                .SetFragment("Post/outline.frag.spv")
                .SetInputs({ fra::PostProcessInput::SceneColor,
                             fra::PostProcessInput::Depth,
                             fra::PostProcessInput::Normal })
                .SetPushConstantSize(sizeof(OutlinePush))
                .Build();
        if (mOutlineEffect)
        {
            OutlinePush o {};
            o.edgeDepthScale  = 90.0f;
            o.edgeNormalScale = 2.0f;
            o.strength        = 1.0f;
            o.reverseZ        = revZ;
            o.edgeColor       = { 0.02f, 0.02f, 0.04f, 1.0f };
            o.edgeWidth       = 1.5f;
            mOutlineEffect->SetPushConstants(o);
            mOutlineEffect->SetEnabled(true);
            insertPost(mOutlineEffect);
        }

        mHeatEffect =
            mServices->GetService<fra::PostProcessBuilder>()
                ->SetName("HeatHaze")
                .SetFragment("Post/heat_haze.frag.spv")
                .SetInputs({ fra::PostProcessInput::SceneColor,
                             fra::PostProcessInput::Depth })
                .SetPushConstantSize(sizeof(HeatPush))
                .Build();
        if (mHeatEffect)
        {
            HeatPush h {};
            h.reverseZ = revZ;
            mHeatPush  = h;
            mHeatEffect->SetPushConstants(mHeatPush);
            mHeatEffect->SetEnabled(false);
            insertPost(mHeatEffect);
        }

        mUnderwaterEffect =
            mServices->GetService<fra::PostProcessBuilder>()
                ->SetName("Underwater")
                .SetFragment("Post/underwater.frag.spv")
                .SetInputs({ fra::PostProcessInput::SceneColor,
                             fra::PostProcessInput::Depth })
                .SetPushConstantSize(sizeof(UnderwaterPush))
                .Build();
        if (mUnderwaterEffect)
        {
            UnderwaterPush u {};
            u.reverseZ      = revZ;
            mUnderwaterPush = u;
            mUnderwaterEffect->SetPushConstants(mUnderwaterPush);
            mUnderwaterEffect->SetEnabled(false);
            insertPost(mUnderwaterEffect);
        }

        mGlowEffect =
            mServices->GetService<fra::PostProcessBuilder>()
                ->SetName("ItemGlow")
                .SetFragment("Post/glow.frag.spv")
                .SetInputs({ fra::PostProcessInput::SceneColor,
                             fra::PostProcessInput::Depth })
                .SetPushConstantSize(sizeof(GlowPush))
                .Build();
        if (mGlowEffect)
        {
            GlowPush g {};
            g.reverseZ = revZ;
            mGlowEffect->SetPushConstants(g);
            mGlowEffect->SetEnabled(false);
            insertPost(mGlowEffect);
        }

        mMuGlowEffect =
            mServices->GetService<fra::PostProcessBuilder>()
                ->SetName("MuItemGlow")
                .SetFragment("Post/mu_item_glow.frag.spv")
                .SetInputs({ fra::PostProcessInput::SceneColor,
                             fra::PostProcessInput::Depth })
                .SetPushConstantSize(sizeof(MuGlowPush))
                .Build();
        if (mMuGlowEffect)
        {
            mMuGlowPush.reverseZ = revZ;
            mMuGlowPush.level    = static_cast<float>(mMuGlowLevel);
            mMuGlowEffect->SetPushConstants(mMuGlowPush);
            mMuGlowEffect->SetEnabled(false);
            insertPost(mMuGlowEffect);
        }

        mGradeEffect =
            mServices->GetService<fra::PostProcessBuilder>()
                ->SetName("ColorGrade")
                .SetFragment("Post/color_grade.frag.spv")
                .SetInputs({ fra::PostProcessInput::SceneColor })
                .SetPushConstantSize(sizeof(GradePush))
                .Build();
        if (mGradeEffect)
        {
            GradePush g {};
            mGradeEffect->SetPushConstants(g);
            mGradeEffect->SetEnabled(false);
            insertPost(mGradeEffect);
        }

        const auto eyeAlbedo =
            mTexturePool->CreateTextureFromFile("./Resources/Textures/eye.png");
        const auto bodyBAlbedo = mTexturePool->CreateTextureFromFile(
            "./Resources/Textures/bodyB.png");
        const auto bodyAAlbedo = mTexturePool->CreateTextureFromFile(
            "./Resources/Textures/bodyA.png");

        mGroundMesh     = createGroundMesh();
        mGroundMaterial = mMaterialPool->Create({
            .albedo          = bodyAAlbedo,
            .albedoFactor    = { 0.75f, 0.82f, 0.55f, 1.0f },
            .roughnessFactor = 0.95f,
            .metalnessFactor = 0.0f,
        });

        auto makeBodyMats = [&](std::uint32_t& eye, std::uint32_t& bodyB,
                                std::uint32_t& bodyA,
                                std::uint32_t  techniqueId) {
            eye   = mMaterialPool->Create({
                .albedo          = eyeAlbedo,
                .roughnessFactor = 1.0f,
                .metalnessFactor = 0.0f,
                .techniqueId     = techniqueId,
            });
            bodyB = mMaterialPool->Create({
                .albedo          = bodyBAlbedo,
                .roughnessFactor = 1.0f,
                .metalnessFactor = 0.0f,
                .techniqueId     = techniqueId,
            });
            bodyA = mMaterialPool->Create({
                .albedo          = bodyAAlbedo,
                .roughnessFactor = 1.0f,
                .metalnessFactor = 0.0f,
                .techniqueId     = techniqueId,
            });
        };

        makeBodyMats(mEyeMaterial, mBodyBMaterial, mBodyAMaterial,
                     mCellTechnique);
        makeBodyMats(mPbrEyeMaterial, mPbrBodyBMaterial, mPbrBodyAMaterial,
                     fra::MaterialTechniqueRegistry::kDefaultTechnique);

        if (mCellEffect)
        {
            mCellEffect->BindMaterial(mEyeMaterial);
            mCellEffect->BindMaterial(mBodyBMaterial);
            mCellEffect->BindMaterial(mBodyAMaterial);
        }
        if (mHeatEffect)
        {
            mHeatEffect->BindMaterial(mEyeMaterial);
            mHeatEffect->BindMaterial(mBodyBMaterial);
            mHeatEffect->BindMaterial(mBodyAMaterial);
        }
        if (mOutlineEffect)
        {
            mOutlineEffect->BindMaterial(mEyeMaterial);
            mOutlineEffect->BindMaterial(mBodyBMaterial);
            mOutlineEffect->BindMaterial(mBodyAMaterial);
        }
        if (mGlowEffect)
        {
            // Item highlight: glow the left (cell) Bulbasaur materials.
            mGlowEffect->BindMaterial(mEyeMaterial);
            mGlowEffect->BindMaterial(mBodyBMaterial);
            mGlowEffect->BindMaterial(mBodyAMaterial);
        }
        if (mMuGlowEffect)
        {
            mMuGlowEffect->BindMaterial(mEyeMaterial);
            mMuGlowEffect->BindMaterial(mBodyBMaterial);
            mMuGlowEffect->BindMaterial(mBodyAMaterial);
        }

        mSkinned = mMeshPool->CreateSkinnedModelFromFile(
            "./Resources/Models/bulbasaur.glb");
        if (mSkinned.meshIds.empty() || mSkinned.skeleton.JointCount() == 0)
        {
            std::cerr << "Failed to load bulbasaur.glb as a skinned model\n";
        }
        else
        {
            std::cout << "bulbasaur meshes: " << mSkinned.meshIds.size()
                      << " joints: " << mSkinned.skeleton.JointCount()
                      << " clips: " << mSkinned.clips.size() << '\n';
            for (const auto& clip : mSkinned.clips)
                std::cout << "  clip: " << clip.name << " (" << clip.duration
                          << "s)\n";
            mIdleClip = findClip(mSkinned, "idle");
            if (!mIdleClip && !mSkinned.clips.empty())
                mIdleClip = &mSkinned.clips.front();
        }

        mLightService->AddLight(fra::MakeDirectionalLight(
            glm::vec3(-0.4f, -1.0f, -0.35f), glm::vec3(1.0f, 0.98f, 0.92f),
            1.6f));
        mLightService->AddLight(fra::MakePointLight(
            glm::vec3(2.2f, 2.4f, 2.0f), glm::vec3(0.85f, 0.95f, 0.7f), 6.0f,
            8.0f));

        mMagic.origin         = glm::vec3(-1.2f, 0.55f, 0.0f);
        mMagic.velocity       = glm::vec3(0.0f, 1.4f, 0.0f);
        mMagic.velocityJitter = glm::vec3(0.45f, 0.3f, 0.45f);
        mMagic.spawnRate      = 28.0f;
        mMagic.lifetime       = 0.85f;
        mMagic.size0          = 0.18f;
        mMagic.size1          = 0.04f;
        mMagic.color0         = glm::vec4(0.45f, 1.0f, 0.55f, 1.0f);
        mMagic.color1         = glm::vec4(0.1f, 0.4f, 0.2f, 0.0f);
        mMagic.blend          = fra::BillboardBlend::Additive;

        const glm::vec3 firePos { 0.0f, 0.08f, 1.55f };
        mFire.origin         = firePos;
        mFire.velocity       = glm::vec3(0.0f, 1.7f, 0.0f);
        mFire.velocityJitter = glm::vec3(0.28f, 0.55f, 0.28f);
        mFire.spawnRate      = 55.0f;
        mFire.lifetime       = 0.5f;
        mFire.size0          = 0.28f;
        mFire.size1          = 0.06f;
        mFire.color0         = glm::vec4(1.0f, 0.72f, 0.18f, 1.0f);
        mFire.color1         = glm::vec4(0.55f, 0.06f, 0.0f, 0.0f);
        mFire.blend          = fra::BillboardBlend::Additive;
        mFire.maxParticles   = 128;

        mEmbers.origin         = firePos;
        mEmbers.velocity       = glm::vec3(0.0f, 2.4f, 0.0f);
        mEmbers.velocityJitter = glm::vec3(0.55f, 0.8f, 0.55f);
        mEmbers.spawnRate      = 14.0f;
        mEmbers.lifetime       = 0.9f;
        mEmbers.size0          = 0.05f;
        mEmbers.size1          = 0.01f;
        mEmbers.color0         = glm::vec4(1.0f, 0.85f, 0.35f, 1.0f);
        mEmbers.color1         = glm::vec4(1.0f, 0.2f, 0.0f, 0.0f);
        mEmbers.blend          = fra::BillboardBlend::Additive;
        mEmbers.maxParticles   = 64;

        mSmoke.origin         = firePos + glm::vec3(0.0f, 0.25f, 0.0f);
        mSmoke.velocity       = glm::vec3(0.0f, 0.7f, 0.0f);
        mSmoke.velocityJitter = glm::vec3(0.2f, 0.15f, 0.2f);
        mSmoke.spawnRate      = 10.0f;
        mSmoke.lifetime       = 1.6f;
        mSmoke.size0          = 0.18f;
        mSmoke.size1          = 0.55f;
        mSmoke.color0         = glm::vec4(0.12f, 0.11f, 0.1f, 0.35f);
        mSmoke.color1         = glm::vec4(0.08f, 0.08f, 0.08f, 0.0f);
        mSmoke.blend          = fra::BillboardBlend::Alpha;
        mSmoke.maxParticles   = 48;

        mFireLight =
            fra::MakePointLight(firePos + glm::vec3(0.0f, 0.35f, 0.0f),
                                glm::vec3(1.0f, 0.45f, 0.12f), 6.0f, 7.0f);
        mFireLightIndex = mLightService->AddLight(mFireLight);

        mFont = fra::FontAtlas::Create(
            *mTexturePool, "./Resources/Fonts/NotoSans-Regular.ttf");
        if (!mFont.Valid())
            std::cerr << "Failed to load NotoSans-Regular.ttf\n";

        buildSceneInstances();
        updateTitle();

        std::cout
            << "CellBulbasaur — left: cell  right: PBR\n"
               "F4 cell | F5 outline | F6 grade | F7 underwater | F8 heat\n"
               "F9 item glow | F12 Mu glow (+N) | [ ] change +level\n"
               "F10 ground triplanar | F11 eyes unlit\n"
               "RMB look | WASD move | Space/Q up | Ctrl/E down\n";
    }

    void Update() override
    {
        const float dt = mWindow->GetDeltaTime();
        mEffectTime += dt;
        updateCamera(dt);

        if (mHeatEffect && mHeatEffect->Enabled())
        {
            mHeatPush.time = mEffectTime;
            mHeatEffect->SetPushConstants(mHeatPush);
        }
        if (mUnderwaterEffect && mUnderwaterEffect->Enabled())
        {
            mUnderwaterPush.time = mEffectTime;
            mUnderwaterEffect->SetPushConstants(mUnderwaterPush);
        }
        if (mMuGlowEffect && mMuGlowEffect->Enabled())
        {
            mMuGlowPush.time = mEffectTime;
            mMuGlowEffect->SetPushConstants(mMuGlowPush);
        }

        mRenderer->BeginFrame();

        const glm::vec3 forward = cameraForward();
        mRenderer->UpdateCamera(
            mCameraPos, mCameraPos + forward, glm::vec3(0.0f, 1.0f, 0.0f));

        const auto jointCount = mSkinned.skeleton.JointCount();
        if (jointCount > 0)
        {
            fra::LocalPose local;
            if (mIdleClip)
            {
                mAnimTime += dt;
                local = fra::SampleClip(
                    mSkinned.skeleton, *mIdleClip, mAnimTime, true);
            }
            else
            {
                local = fra::RestLocalPose(mSkinned.skeleton);
            }
            mRenderer->UploadBoneMatrices(
                fra::PoseToSkinMatrices(mSkinned.skeleton, local));
        }

        std::vector<fra::SceneInstanceUpload> instances;
        instances.reserve(mInstances.size());
        for (const auto& inst : mInstances)
        {
            instances.push_back({
                .model       = inst.model,
                .meshId      = inst.meshId,
                .materialId  = inst.materialId,
                .entityId    = inst.entityId,
                .castShadows = inst.castShadows,
                .boneOffset  = inst.boneOffset,
                .boneCount   = inst.boneCount,
            });
        }
        mRenderer->UploadSceneInstances(instances);

        mHpPulse += dt;
        auto&       bb     = mRenderer->GetBillboardDraw();
        const float cellHp = 0.45f + 0.45f * std::sin(mHpPulse * 0.8f);
        const float pbrHp  = 0.70f + 0.20f * std::sin(mHpPulse * 0.5f + 1.2f);
        bb.HealthBar(glm::vec3(-1.2f, 1.25f, 0.0f), 0.85f, 0.08f, cellHp,
                     glm::vec4(0.08f, 0.08f, 0.08f, 0.85f),
                     glm::vec4(0.25f, 0.85f, 0.35f, 1.0f));
        bb.HealthBar(glm::vec3(1.2f, 1.25f, 0.0f), 0.85f, 0.08f, pbrHp,
                     glm::vec4(0.08f, 0.08f, 0.08f, 0.85f),
                     glm::vec4(0.85f, 0.35f, 0.25f, 1.0f));
        bb.Text(glm::vec3(-1.2f, 1.42f, 0.0f), "Cell", mFont, 0.16f,
                glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 2.0f,
                glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        bb.Text(glm::vec3(1.2f, 1.42f, 0.0f), "PBR", mFont, 0.16f,
                glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 2.0f,
                glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        mMagic.Tick(dt, bb);
        mFire.Tick(dt, bb);
        mEmbers.Tick(dt, bb);
        mSmoke.Tick(dt, bb);

        if (mFireLightIndex >= 0)
        {
            const float flicker = 0.75f + 0.25f * std::sin(mHpPulse * 11.0f) +
                                  0.12f * std::sin(mHpPulse * 23.0f);
            auto        lit     = mFireLight;
            lit.intensity       = 6.0f * flicker;
            mLightService->UpdateLight(
                static_cast<std::uint32_t>(mFireLightIndex),
                lit);
        }

        mRenderer->EndFrame();
    }

  private:
    static constexpr float kMoveSpeed        = 6.0f;
    static constexpr float kMouseSensitivity = 0.12f;
    static constexpr float kModelScale       = 100.0f;

    struct Instance
    {
        glm::mat4     model {};
        std::uint32_t meshId      = 0;
        std::uint32_t materialId  = 0;
        std::uint32_t entityId    = 0;
        bool          castShadows = true;
        std::uint32_t boneOffset  = fra::kNoSkin;
        std::uint32_t boneCount   = 0;
    };

    static const fra::AnimationClip* findClip(const fra::SkinnedModel& model,
                                              std::string_view         needle)
    {
        for (const auto& clip : model.clips)
        {
            if (clip.name.find(needle) != std::string::npos)
                return &clip;
        }
        return nullptr;
    }

    std::uint32_t materialForMesh(std::size_t index, bool cellShaded) const
    {
        switch (index % 3)
        {
            case 0:
                return cellShaded ? mEyeMaterial : mPbrEyeMaterial;
            case 1:
                return cellShaded ? mBodyBMaterial : mPbrBodyBMaterial;
            default:
                return cellShaded ? mBodyAMaterial : mPbrBodyAMaterial;
        }
    }

    std::uint32_t createGroundMesh()
    {
        constexpr float half = 8.0f;
        const auto      tint = glm::vec3(0.72f, 0.78f, 0.55f);
        const auto      up   = glm::vec3(0.0f, 1.0f, 0.0f);
        const auto      tan  = glm::vec3(1.0f, 0.0f, 0.0f);

        const std::vector<fra::Vertex> vertices = {
            { { -half, 0.0f, -half }, tint, up, tan, { 0.0f, 0.0f } },
            { { half, 0.0f, -half }, tint, up, tan, { 1.0f, 0.0f } },
            { { half, 0.0f, half }, tint, up, tan, { 1.0f, 1.0f } },
            { { -half, 0.0f, half }, tint, up, tan, { 0.0f, 1.0f } },
        };
        const std::vector<std::uint32_t> indices = { 0, 3, 2, 0, 2, 1 };
        return mMeshPool->CreateMesh(vertices, indices);
    }

    void buildSceneInstances()
    {
        mInstances.clear();
        std::uint32_t nextEntity = 1;

        {
            Instance ground {};
            ground.model =
                glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.2f, 0.0f));
            ground.meshId      = mGroundMesh;
            ground.materialId  = mGroundMaterial;
            ground.entityId    = nextEntity++;
            ground.castShadows = true;
            mInstances.push_back(ground);
        }

        const auto joints       = mSkinned.skeleton.JointCount();
        auto       addBulbasaur = [&](float x, bool cellShaded) {
            auto model =
                glm::translate(glm::mat4(1.0f), glm::vec3(x, 0.0f, 0.0f));
            model = glm::rotate(model, glm::radians(90.0f),
                                glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::scale(model, glm::vec3(kModelScale));
            for (std::size_t i = 0; i < mSkinned.meshIds.size(); ++i)
            {
                Instance inst {};
                inst.model      = model;
                inst.meshId     = mSkinned.meshIds[i];
                inst.materialId = materialForMesh(i, cellShaded);
                inst.entityId   = nextEntity++;
                inst.boneOffset = joints > 0 ? 0u : fra::kNoSkin;
                inst.boneCount  = joints;
                mInstances.push_back(inst);
            }
        };

        addBulbasaur(-1.2f, true);
        addBulbasaur(1.2f, false);
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

    void applyMuGlowLevel()
    {
        mMuGlowPush.level = static_cast<float>(mMuGlowLevel);
        if (mMuGlowEffect)
        {
            mMuGlowPush.time = mEffectTime;
            mMuGlowEffect->SetPushConstants(mMuGlowPush);
            if (!mMuGlowEffect->Enabled())
                mMuGlowEffect->SetEnabled(true);
        }
        static constexpr const char* kTier[] = {
            "+0..2 none",
            "+0..2 none",
            "+0..2 none",
            "+3/+4 red tint",
            "+3/+4 red tint",
            "+5/+6 blue tint",
            "+5/+6 blue tint",
            "+7/+8 soft glow",
            "+7/+8 soft glow",
            "+9/+10/+11 strong",
            "+9/+10/+11 strong",
            "+11 white spark",
            "+12 bright flash",
            "+13 wave flash",
        };
        std::cout << "Mu glow level +" << mMuGlowLevel << " ("
                  << kTier[mMuGlowLevel] << ")\n";
        updateTitle();
    }

    void updateTitle()
    {
        auto flag = [](const skr::Arc<fra::PostProcess>& e) {
            return e && e->Enabled() ? '1' : '0';
        };
        mFreyaOptions->title =
            std::string("CellBulbasaur | C") + flag(mCellEffect) + " O" +
            flag(mOutlineEffect) + " G" + flag(mGradeEffect) + " U" +
            flag(mUnderwaterEffect) + " H" + flag(mHeatEffect) + " L" +
            flag(mGlowEffect) + " M" + flag(mMuGlowEffect) + "+" +
            std::to_string(mMuGlowLevel) + (mGroundTriplanar ? " tri" : "") +
            (mEyesUnlit ? " unlit" : "");
    }

    skr::Arc<skr::ServiceProvider> mServices;
    skr::Arc<fra::PostProcess>     mCellEffect;
    skr::Arc<fra::PostProcess>     mOutlineEffect;
    skr::Arc<fra::PostProcess>     mGradeEffect;
    skr::Arc<fra::PostProcess>     mUnderwaterEffect;
    skr::Arc<fra::PostProcess>     mHeatEffect;
    skr::Arc<fra::PostProcess>     mGlowEffect;
    skr::Arc<fra::PostProcess>     mMuGlowEffect;
    HeatPush                       mHeatPush {};
    UnderwaterPush                 mUnderwaterPush {};
    MuGlowPush                     mMuGlowPush {};
    int                            mMuGlowLevel = 13;
    std::uint32_t                  mCellTechnique      = 0;
    std::uint32_t                  mTriplanarTechnique = 0;
    std::uint32_t                  mUnlitTechnique     = 0;
    bool                           mGroundTriplanar    = false;
    bool                           mEyesUnlit          = false;
    float                          mEffectTime         = 0.0f;
    skr::Arc<fra::MeshPool>        mMeshPool;
    skr::Arc<fra::TexturePool>     mTexturePool;
    skr::Arc<fra::MaterialPool>    mMaterialPool;
    skr::Arc<fra::LightService>    mLightService;
    skr::Arc<fra::FreyaOptions>    mFreyaOptions;

    std::uint32_t             mGroundMesh       = 0;
    std::uint32_t             mGroundMaterial   = 0;
    std::uint32_t             mEyeMaterial      = 0;
    std::uint32_t             mBodyAMaterial    = 0;
    std::uint32_t             mBodyBMaterial    = 0;
    std::uint32_t             mPbrEyeMaterial   = 0;
    std::uint32_t             mPbrBodyAMaterial = 0;
    std::uint32_t             mPbrBodyBMaterial = 0;
    fra::SkinnedModel         mSkinned;
    const fra::AnimationClip* mIdleClip = nullptr;
    float                     mAnimTime = 0.0f;
    fra::ParticleEmitter      mMagic;
    fra::ParticleEmitter      mFire;
    fra::ParticleEmitter      mEmbers;
    fra::ParticleEmitter      mSmoke;
    fra::Light                mFireLight;
    std::int32_t              mFireLightIndex = -1;
    fra::FontAtlas            mFont;
    float                     mHpPulse = 0.0f;
    std::vector<Instance>     mInstances;

    glm::vec3 mCameraPos { 0.0f, 1.6f, 5.2f };
    float     mYaw      = -90.0f;
    float     mPitch    = -8.0f;
    bool      mLookHeld = false;

    std::unordered_set<std::uint32_t> mKeysHeld;
};

int main(int, const char**)
{
    const auto app =
        skr::ApplicationBuilder()
            .WithExtension<fra::FreyaExtension>([](fra::FreyaExtension freya) {
                freya.WithOptions([](fra::FreyaOptionsBuilder& o) {
                    o.SetTitle("CellBulbasaur")
                        .SetWidth(1600)
                        .SetHeight(900)
                        .SetFullscreen(false)
                        .SetVSync(true)
                        .WithReverseZ()
                        .SetSampleCount(1)
                        .SetIblIntensity(0.0f)
                        .SetExposure(0.85f)
                        .SetEnvironmentMapPath("")
                        .SetShadowQuality(fra::ShadowQuality::Ultra)
                        .SetTaaQuality(fra::TaaQuality::Ultra)
                        .SetBloomQuality(fra::BloomQuality::Ultra)
                        .SetSsaoQuality(fra::SsaoQuality::Ultra);
                });
            })
            .Build<MainApp>();

    app->Run();
    return 0;
}
