#include <Freya/Advanced.hpp>

#include <FreyaExamples/AnimClipUtil.hpp>
#include <FreyaExamples/CullAabbDebugDraw.hpp>
#include <FreyaExamples/DebugOverlay.hpp>
#include <FreyaExamples/ExampleLogging.hpp>
#include <FreyaExamples/FlyCam.hpp>
#include <FreyaExamples/GroundMesh.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
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

    // Mu Online upgrade glow (+0 … +13). Matches
    // Shaders/Post/mu_item_glow.frag.
    struct MuGlowPush
    {
        float time      = 0.0f;
        float level     = 13.0f;
        float intensity = 1.0f;
        float reverseZ  = 0.0f;
        float radius    = 7.0f;
        float waveSpeed = 1.0f;
        float _pad0     = 0.0f;
        float _pad1     = 0.0f;
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
    explicit MainApp(const fra::Ref<fra::ServiceProvider>& serviceProvider) :
        AbstractApplication(serviceProvider)
    {
        auto windowServices = GetMainServiceProvider();
        mMeshPool           = serviceProvider->GetService<fra::MeshPool>();
        mTexturePool        = serviceProvider->GetService<fra::TexturePool>();
        mMaterialPool       = serviceProvider->GetService<fra::MaterialPool>();
        mLightService       = windowServices->GetService<fra::LightService>();
        mFreyaOptions       = windowServices->GetService<fra::FreyaOptions>();
        mServices           = windowServices;
    }

    void StartUp() override
    {
        mCam.window     = mWindow;
        mCam.moveSpeed  = 6.0f;
        mCam.cameraPos  = { 0.0f, 1.6f, 5.2f };
        mCam.yaw        = -90.0f;
        mCam.pitch      = -8.0f;
        mCam.blockMouse = [this] { return mOverlay.WantsCaptureMouse(); };
        mCam.BindInput(*mEventManager);

        mEventManager->Subscribe<fra::KeyReleasedEvent>(
            [this](const fra::KeyReleasedEvent& event) {
                if (event.key == fra::KeyCode::F3)
                {
                    const bool enabled = !mOverlay.ShowCullAabbs();
                    mOverlay.SetShowCullAabbs(enabled);
                    if (enabled)
                        mRenderer->SetDebugDrawEnabled(true);
                    std::cout
                        << "Cull AABB debug draw: " << (enabled ? "on" : "off")
                        << " (magenta = eye submeshes; also toggleable "
                           "in ImGui: Freya Debug > GPU Cull)\n";
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
                    mEyesUnlit      = !mEyesUnlit;
                    auto setEyeTech = [&](fra::MaterialHandle id,
                                          std::uint32_t       cellOrDefault) {
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

        mRenderer->ClearProjections();

        auto techniques =
            mServices->GetService<fra::MaterialTechniqueRegistry>();
        mCellTechnique =
            techniques->Register("CellGBuffer", "Cell/gbuffer_cell.frag.spv");
        mTriplanarTechnique =
            techniques->Register("Triplanar", "Material/triplanar.frag.spv");
        mUnlitTechnique = techniques->Register(
            "UnlitEmissive", "Material/unlit_emissive.frag.spv");

        auto lighting = mServices->GetService<fra::LightingTechniqueRegistry>();
        lighting->SetFragment("Cell/lighting_cell.frag.spv");

        mRenderer->RebuildSwapChain();

        const auto revZ       = mFreyaOptions->ReverseZ ? 1.0f : 0.0f;
        auto       insertPost = [&](skr::Arc<fra::PostProcess> effect) {
            if (effect)
                fra::Advanced(*mRenderer)
                    .InsertFrameStage("BillboardVfx", effect->MakeStage());
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

        mGroundMesh = FreyaExamples::CreateGroundPlane(
            *mMeshPool, 8.0f, glm::vec3(0.72f, 0.78f, 0.55f));
        mGroundMaterial = mMaterialPool->Create({
            .albedo          = bodyAAlbedo,
            .albedoFactor    = { 0.75f, 0.82f, 0.55f, 1.0f },
            .roughnessFactor = 0.95f,
            .metalnessFactor = 0.0f,
        });

        auto makeBodyMats =
            [&](fra::MaterialHandle& eye, fra::MaterialHandle& bodyB,
                fra::MaterialHandle& bodyA, std::uint32_t techniqueId) {
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
            mCellEffect->BindMaterial(mEyeMaterial.Id());
            mCellEffect->BindMaterial(mBodyBMaterial.Id());
            mCellEffect->BindMaterial(mBodyAMaterial.Id());
        }
        if (mHeatEffect)
        {
            mHeatEffect->BindMaterial(mEyeMaterial.Id());
            mHeatEffect->BindMaterial(mBodyBMaterial.Id());
            mHeatEffect->BindMaterial(mBodyAMaterial.Id());
        }
        if (mOutlineEffect)
        {
            mOutlineEffect->BindMaterial(mEyeMaterial.Id());
            mOutlineEffect->BindMaterial(mBodyBMaterial.Id());
            mOutlineEffect->BindMaterial(mBodyAMaterial.Id());
        }
        if (mGlowEffect)
        {
            mGlowEffect->BindMaterial(mEyeMaterial.Id());
            mGlowEffect->BindMaterial(mBodyBMaterial.Id());
            mGlowEffect->BindMaterial(mBodyAMaterial.Id());
        }
        if (mMuGlowEffect)
        {
            mMuGlowEffect->BindMaterial(mEyeMaterial.Id());
            mMuGlowEffect->BindMaterial(mBodyBMaterial.Id());
            mMuGlowEffect->BindMaterial(mBodyAMaterial.Id());
        }

        mSkinned = mMeshPool->CreateSkinnedModelFromFile(
            "./Resources/Models/bulbasaur.glb");
        if (mSkinned.submeshes.empty() || mSkinned.skeleton.JointCount() == 0)
        {
            std::cerr << "Failed to load bulbasaur.glb as a skinned model\n";
        }
        else
        {
            std::cout << "bulbasaur submeshes: " << mSkinned.submeshes.size()
                      << " joints: " << mSkinned.skeleton.JointCount()
                      << " clips: " << mSkinned.clips.size() << '\n';
            for (const auto& clip : mSkinned.clips)
                std::cout << "  clip: " << clip.name << " (" << clip.duration
                          << "s)\n";
            mIdleClip = FreyaExamples::FindClipContaining(mSkinned, "idle");
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
        mFireLightHandle = mLightService->AddLight(mFireLight);

        mFont = fra::FontAtlas::Create(
            *mTexturePool, "./Resources/Fonts/NotoSans-Regular.ttf");
        if (!mFont.Valid())
            std::cerr << "Failed to load NotoSans-Regular.ttf\n";

        buildScene();
        updateTitle();

        // After RebuildSwapChain / InsertFrameStage so ImGui binds the final
        // UI render pass.
        if (mPlatform)
            mOverlay.Init(*mRenderer, *mWindow, *mPlatform);
        mOverlay.SetCullDumpExampleName("CellBulbasaur");

        std::cout
            << "CellBulbasaur — left: cell  right: PBR\n"
               "F3 cull AABBs (also in ImGui: Freya Debug > GPU Cull)\n"
               "F4 cell | F5 outline | F6 grade | F7 underwater | F8 heat\n"
               "F9 item glow | F12 Mu glow (+N) | [ ] change +level\n"
               "F10 ground triplanar | F11 eyes unlit\n"
               "RMB look | WASD move | Space/Q up | Ctrl/E down\n"
               "ImGui: Freya Debug panel (timing / quality / SSAO views)\n";
    }

    void Update() override
    {
        mOverlay.MarkUpdateStart();
        mOverlay.BeginFrame();

        const float dt = mWindow->GetDeltaTime();
        mEffectTime += dt;
        mCam.Update(dt);

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

        mCam.Apply(*mRenderer);

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

        mScene.Upload(*mRenderer);

        if (mOverlay.ShowCullAabbs())
            drawCullAabbs();

        mHpPulse += dt;
        auto&       bb     = mRenderer->GetBillboardDraw();
        const float cellHp = 0.45f + 0.45f * std::sin(mHpPulse * 0.8f);
        const float pbrHp  = 0.70f + 0.20f * std::sin(mHpPulse * 0.5f + 1.2f);
        bb.HealthBar(glm::vec3(-1.2f, 1.25f, 0.0f), 0.85f, 0.08f, cellHp,
                     glm::vec4(0.08f, 0.08f, 0.08f, 0.85f),
                     glm::vec4(0.25f, 0.85f, 0.35f, 1.0f),
                     fra::BillboardAlign::Screen);
        bb.HealthBar(glm::vec3(1.2f, 1.25f, 0.0f), 0.85f, 0.08f, pbrHp,
                     glm::vec4(0.08f, 0.08f, 0.08f, 0.85f),
                     glm::vec4(0.85f, 0.35f, 0.25f, 1.0f),
                     fra::BillboardAlign::Cylindrical);
        bb.Text(glm::vec3(-1.2f, 1.42f, 0.0f), "Cell", mFont, 0.16f,
                glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 2.0f,
                glm::vec4(0.0f, 0.0f, 0.0f, 1.0f), fra::BillboardAlign::Screen);
        bb.Text(glm::vec3(1.2f, 1.42f, 0.0f), "PBR", mFont, 0.16f,
                glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), 2.0f,
                glm::vec4(0.0f, 0.0f, 0.0f, 1.0f),
                fra::BillboardAlign::Cylindrical);
        mMagic.Tick(dt, bb);
        mFire.Tick(dt, bb);
        mEmbers.Tick(dt, bb);
        mSmoke.Tick(dt, bb);

        if (mFireLightHandle)
        {
            const float flicker = 0.75f + 0.25f * std::sin(mHpPulse * 11.0f) +
                                  0.12f * std::sin(mHpPulse * 23.0f);
            auto        lit     = mFireLight;
            lit.intensity       = 6.0f * flicker;
            mLightService->UpdateLight(mFireLightHandle, lit);
        }

        const float cpuFrameMs  = dt * 1000.f;
        const float cpuUpdateMs = mOverlay.ElapsedUpdateMs();
        mOverlay.Draw(*mRenderer, *mFreyaOptions, cpuFrameMs, cpuUpdateMs);
        mOverlay.EndFrame(*mRenderer);
    }

  private:
    // After nonBoneParent fix, rest skin ≈ I (mesh-local units, ~0.5 tall).
    // Old scale≈100 was compensating a missing parent scale that shrank the
    // skinned mesh via IBM; AABB used raw verts and looked huge. Scale freely
    // now — cull AABBs share the instance model matrix.
    static constexpr float kModelScale = 1.0f;

    fra::MaterialHandle materialForMesh(std::size_t index,
                                        bool        cellShaded) const
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

    void buildScene()
    {
        mScene.Clear();
        mIsEye.clear();
        std::uint32_t nextEntity = 1;

        {
            fra::Scene::Instance ground {};
            ground.transform = fra::SceneTransform::FromMatrix(
                glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.2f, 0.0f)));
            ground.mesh        = mGroundMesh;
            ground.material    = mGroundMaterial;
            ground.entityId    = nextEntity++;
            ground.castShadows = true;
            ground.mobility    = fra::Mobility::Static;
            const auto id      = mScene.Add(ground);
            ensureIsEye(id, false);
        }

        const auto joints       = mSkinned.skeleton.JointCount();
        auto       addBulbasaur = [&](float x, bool cellShaded) {
            auto model =
                glm::translate(glm::mat4(1.0f), glm::vec3(x, 0.0f, 0.0f));
            model = glm::scale(model, glm::vec3(kModelScale));
            for (std::size_t i = 0; i < mSkinned.submeshes.size(); ++i)
            {
                fra::Scene::Instance inst {};
                inst.transform  = fra::SceneTransform::FromMatrix(model);
                inst.mesh       = mSkinned.submeshes[i].mesh;
                inst.material   = materialForMesh(i, cellShaded);
                inst.entityId   = nextEntity++;
                inst.boneOffset = joints > 0 ? 0u : fra::kNoSkin;
                inst.boneCount  = joints;
                inst.mobility   = fra::Mobility::Dynamic;
                const auto id   = mScene.Add(inst);
                ensureIsEye(id, (i % 3) == 0);
            }
        };

        addBulbasaur(-1.2f, true);
        addBulbasaur(1.2f, false);
    }

    void ensureIsEye(fra::Scene::InstanceId id, bool isEye)
    {
        if (id >= mIsEye.size())
            mIsEye.resize(id + 1, false);
        mIsEye[id] = isEye;
    }

    /**
     * @brief Draws the exact world-space AABB the GPU cull compute shader
     * (Shaders/GpuDriven/CullFrustum.comp) tests each instance against:
     * MeshPool's registered mesh-local aabbMin/aabbMax transformed by the
     * instance's model matrix. Eye submeshes (entityId 2/5 in the
     * cell_eyes_false_cull fixture) are highlighted in magenta so their
     * AABB — inflated for skinned cull conservatism — is easy to pick out
     * against the body submeshes (cyan). Toggle via the "Debug draw"
     * checkbox in the ImGui "Freya Debug" panel (or F3).
     */
    void drawCullAabbs()
    {
        auto& dd = mRenderer->GetDebugDraw();
        mScene.ForEach([&](fra::Scene::InstanceId      id,
                           const fra::Scene::Instance& inst) {
            const bool      isEye = id < mIsEye.size() && mIsEye[id];
            const glm::vec4 color = isEye ? glm::vec4(1.0f, 0.15f, 0.85f, 1.0f)
                                          : glm::vec4(0.2f, 0.9f, 1.0f, 0.6f);
            FreyaExamples::DrawCullAabb(
                dd, *mMeshPool, inst.mesh, inst.transform.ToMatrix(), color);
        });
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
            "+0..2 none",        "+0..2 none",        "+0..2 none",
            "+3/+4 red tint",    "+3/+4 red tint",    "+5/+6 blue tint",
            "+5/+6 blue tint",   "+7/+8 soft glow",   "+7/+8 soft glow",
            "+9/+10/+11 strong", "+9/+10/+11 strong", "+11 white spark",
            "+12 bright flash",  "+13 wave flash",
        };
        std::cout << "Mu glow level +" << mMuGlowLevel << " ("
                  << kTier[mMuGlowLevel] << ")\n";
        updateTitle();
    }

    void updateTitle()
    {
        auto flag = [](const fra::Ref<fra::PostProcess>& e) {
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

    fra::Ref<skr::ServiceProvider> mServices;
    fra::Ref<fra::PostProcess>     mCellEffect;
    fra::Ref<fra::PostProcess>     mOutlineEffect;
    fra::Ref<fra::PostProcess>     mGradeEffect;
    fra::Ref<fra::PostProcess>     mUnderwaterEffect;
    fra::Ref<fra::PostProcess>     mHeatEffect;
    fra::Ref<fra::PostProcess>     mGlowEffect;
    fra::Ref<fra::PostProcess>     mMuGlowEffect;
    HeatPush                       mHeatPush {};
    UnderwaterPush                 mUnderwaterPush {};
    MuGlowPush                     mMuGlowPush {};
    int                            mMuGlowLevel        = 13;
    std::uint32_t                  mCellTechnique      = 0;
    std::uint32_t                  mTriplanarTechnique = 0;
    std::uint32_t                  mUnlitTechnique     = 0;
    bool                           mGroundTriplanar    = false;
    bool                           mEyesUnlit          = false;
    float                          mEffectTime         = 0.0f;
    fra::Ref<fra::MeshPool>        mMeshPool;
    fra::Ref<fra::TexturePool>     mTexturePool;
    fra::Ref<fra::MaterialPool>    mMaterialPool;
    fra::Ref<fra::LightService>    mLightService;
    fra::Ref<fra::FreyaOptions>    mFreyaOptions;

    fra::MeshHandle           mGroundMesh {};
    fra::MaterialHandle       mGroundMaterial {};
    fra::MaterialHandle       mEyeMaterial {};
    fra::MaterialHandle       mBodyAMaterial {};
    fra::MaterialHandle       mBodyBMaterial {};
    fra::MaterialHandle       mPbrEyeMaterial {};
    fra::MaterialHandle       mPbrBodyAMaterial {};
    fra::MaterialHandle       mPbrBodyBMaterial {};
    fra::SkinnedModel         mSkinned;
    const fra::AnimationClip* mIdleClip = nullptr;
    float                     mAnimTime = 0.0f;
    fra::ParticleEmitter      mMagic;
    fra::ParticleEmitter      mFire;
    fra::ParticleEmitter      mEmbers;
    fra::ParticleEmitter      mSmoke;
    fra::Light                mFireLight;
    fra::LightHandle          mFireLightHandle {};
    fra::FontAtlas            mFont;
    float                     mHpPulse = 0.0f;
    fra::Scene                mScene;
    std::vector<bool>         mIsEye;

    FreyaExamples::FlyCam       mCam;
    FreyaExamples::DebugOverlay mOverlay;
};

int main(int, const char**)
{
    return fra::RunApp<MainApp>(
        [](fra::FreyaOptionsBuilder& o) {
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
        },
        [](skr::LoggingExtension& l) {
            FreyaExamples::ConfigureLogging(l, "CellBulbasaur.log");
        });
}
