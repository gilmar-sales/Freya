#include <Freya/Freya.hpp>

#include <FreyaExamples/CullAabbDebugDraw.hpp>
#include <FreyaExamples/DebugOverlay.hpp>
#include <FreyaExamples/ExampleLogging.hpp>
#include <FreyaExamples/FlyCam.hpp>
#include <FreyaExamples/GroundMesh.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace
{
    enum class UiScreen : int
    {
        None      = 0,
        Inventory = 1,
        Dialogue  = 2,
        Chat      = 3,
    };

    enum class EquipSlot : int
    {
        Head   = 0,
        Chest  = 1,
        Hands  = 2,
        Legs   = 3,
        Feet   = 4,
        Weapon = 5,
        Count  = 6,
    };

    const char* EquipSlotName(EquipSlot slot)
    {
        switch (slot)
        {
            case EquipSlot::Head:
                return "Head";
            case EquipSlot::Chest:
                return "Chest";
            case EquipSlot::Hands:
                return "Hands";
            case EquipSlot::Legs:
                return "Legs";
            case EquipSlot::Feet:
                return "Feet";
            case EquipSlot::Weapon:
                return "Weapon";
            default:
                return "Slot";
        }
    }

    struct InvItem
    {
        std::string        name;
        fra::TextureHandle icon {};
        int                stack = 1;
    };

    struct Ability
    {
        const char*        name        = "";
        const char*        description = "";
        const char*        hotkey      = "";
        fra::KeyCode       key         = fra::KeyCode::Unknown;
        fra::TextureHandle icon {};
        float              duration  = 4.f;
        float              remaining = 0.f;
    };

    fra::TextureHandle MakeSolidTexture(fra::TexturePool& pool, std::uint8_t r,
                                        std::uint8_t g, std::uint8_t b,
                                        std::uint32_t size = 64)
    {
        std::vector<std::uint8_t> pixels(size * size * 4);
        for (std::uint32_t i = 0; i < size * size; ++i)
        {
            pixels[i * 4 + 0] = r;
            pixels[i * 4 + 1] = g;
            pixels[i * 4 + 2] = b;
            pixels[i * 4 + 3] = 255;
        }
        return pool.CreateTextureFromMemory(pixels.data(), size, size, 4, 1);
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
    }

    ~MainApp() override
    {
        if (mRenderer && mDollPreview)
            mRenderer->RemoveModelPreview(mDollPreview.get());
    }

    void StartUp() override
    {
        mMainCam.window     = mWindow;
        mMainCam.blockMouse = [this] {
            return mRenderer->GetUiContext().WantCaptureMouse() ||
                   mOverlay.WantsCaptureMouse();
        };
        mMainCam.blockKeyboard = [this] {
            return mRenderer->GetUiContext().WantTextInput();
        };
        mMainCam.BindInput(*mEventManager);

        if (mPlatform)
            mOverlay.Init(*mRenderer, *mWindow, *mPlatform);
        mOverlay.SetCullDumpExampleName("GameUiDemo");

        mEventManager->Subscribe<fra::KeyReleasedEvent>(
            [this](const fra::KeyReleasedEvent& event) {
                if (mRenderer && mRenderer->GetUiContext().WantTextInput())
                    return;
                if (event.key == fra::KeyCode::Escape)
                {
                    if (mScreen != UiScreen::None)
                    {
                        mScreen = UiScreen::None;
                        return;
                    }
                }
                if (event.key == fra::KeyCode::F1)
                {
                    mScreen = mScreen == UiScreen::Inventory
                                  ? UiScreen::None
                                  : UiScreen::Inventory;
                    return;
                }
                if (event.key == fra::KeyCode::F2)
                {
                    mScreen = mScreen == UiScreen::Dialogue
                                  ? UiScreen::None
                                  : UiScreen::Dialogue;
                    return;
                }
                if (event.key == fra::KeyCode::F3)
                {
                    const bool open = mScreen != UiScreen::Chat;
                    mScreen         = open ? UiScreen::Chat : UiScreen::None;
                    if (open)
                        mChatFocusPending = true;
                    return;
                }
                if (mScreen == UiScreen::None)
                {
                    for (std::size_t i = 0; i < mAbilities.size(); ++i)
                    {
                        if (event.key == mAbilities[i].key)
                        {
                            tryActivateAbility(i);
                            return;
                        }
                    }
                }
            });

        mFont = fra::FontAtlas::Create(
            *mTexturePool, "./Resources/Fonts/NotoSans-Regular.ttf");
        if (!mFont.Valid())
            std::cerr << "Failed to load NotoSans-Regular.ttf\n";

        mIconSword  = MakeSolidTexture(*mTexturePool, 180, 80, 40);
        mIconPotion = MakeSolidTexture(*mTexturePool, 40, 160, 90);
        mIconGem    = MakeSolidTexture(*mTexturePool, 60, 100, 200);
        mPortrait   = MakeSolidTexture(*mTexturePool, 90, 70, 55, 128);
        mIconFire   = MakeSolidTexture(*mTexturePool, 220, 70, 30);
        mIconIce    = MakeSolidTexture(*mTexturePool, 80, 160, 230);
        mIconHeal   = MakeSolidTexture(*mTexturePool, 80, 200, 120);
        mIconShield = MakeSolidTexture(*mTexturePool, 180, 160, 60);
        mIconDash   = MakeSolidTexture(*mTexturePool, 140, 100, 220);
        mIconStun   = MakeSolidTexture(*mTexturePool, 200, 120, 40);

        mAbilities = { {
            { "Firebolt", "Hurl a searing bolt. 4s cooldown.", "1",
              fra::KeyCode::Num1, mIconFire, 4.f },
            { "Frost Nova", "Freeze nearby foes. 6s cooldown.", "2",
              fra::KeyCode::Num2, mIconIce, 6.f },
            { "Mend", "Restore a pulse of health. 5s cooldown.", "3",
              fra::KeyCode::Num3, mIconHeal, 5.f },
            { "Bulwark", "Raise a brief barrier. 8s cooldown.", "4",
              fra::KeyCode::Num4, mIconShield, 8.f },
            { "Dash", "Blink forward a short distance. 3s cooldown.", "5",
              fra::KeyCode::Num5, mIconDash, 3.f },
            { "Shockwave", "Stun in a cone ahead. 7s cooldown.", "6",
              fra::KeyCode::Num6, mIconStun, 7.f },
        } };

        mSlots.fill(std::nullopt);
        mSlots[0] = InvItem { "Iron Sword", mIconSword, 1 };
        mSlots[1] = InvItem { "Health Potion", mIconPotion, 3 };
        mSlots[2] = InvItem { "Sapphire", mIconGem, 12 };
        mSlots[5] = InvItem { "Health Potion", mIconPotion, 1 };
        mSlots[8] = InvItem { "Iron Sword", mIconSword, 1 };
        mEquip.fill(std::nullopt);
        mEquip[static_cast<std::size_t>(EquipSlot::Weapon)] =
            InvItem { "Iron Sword", mIconSword, 1 };

        mChatLog = {
            "[System] Welcome to GameUiDemo.",
            "[Hint] Keys 1-6 cast abilities. F1 inventorio, F2 dialogo, F3 "
            "chat.",
            "[Hint] F1 paper-doll: drag orbit, equip slots, Tirar foto.",
            "[Hint] RMB+WASD to look/move; Esc closes UI.",
        };
        mChatInput.clear();

        mGroundMesh = FreyaExamples::CreateGroundPlane(
            *mMeshPool, 40.0f, glm::vec3(0.35f, 0.36f, 0.38f));
        mGroundMaterial = mMaterialPool->Create({});

        mLightService->AddLight(fra::MakeDirectionalLight(
            glm::vec3(-0.3f, -1.0f, -0.2f), glm::vec3(1.0f, 0.96f, 0.9f),
            1.8f));

        setupPaperDoll();
        rebuildScene();

        std::cout
            << "GameUiDemo — native Freya UI (not ImGui)\n"
            << "  1-6 Abilities | F1 Inventory (paper-doll) | F2 Dialogue | "
               "F3 Chat | Esc\n"
            << "  RMB look | WASD move | ImGui debug panel still available\n";
    }

    void Update() override
    {
        mOverlay.MarkUpdateStart();
        mOverlay.BeginFrame();

        const float dt = mWindow->GetDeltaTime();
        mTime += dt;
        for (auto& a : mAbilities)
        {
            if (a.remaining > 0.f)
            {
                a.remaining -= dt;
                if (a.remaining < 0.f)
                    a.remaining = 0.f;
            }
        }
        mMainCam.Update(dt);
        mHpPulse = 0.55f + 0.45f * std::sin(mTime * 0.8f);

        const bool dollActive =
            mPortraitPending || mScreen == UiScreen::Inventory;
        if (mDollPreview)
            mDollPreview->SetActive(dollActive);
        if (dollActive)
            updateDollAnimation(dt);

        mRenderer->BeginFrame();
        mMainCam.Apply(*mRenderer);
        mScene.Upload(*mRenderer);

        drawGameUi(dt);

        const float cpuFrameMs  = mWindow->GetDeltaTime() * 1000.f;
        const float cpuUpdateMs = mOverlay.ElapsedUpdateMs();
        mOverlay.Draw(*mRenderer, *mFreyaOptions, cpuFrameMs, cpuUpdateMs,
                      mLightService.get());
        mOverlay.EndFrame(*mRenderer);

        // After ModelPreviewFrameStage has Recorded at least once.
        maybeCapturePortrait();
    }

  private:
    void setupPaperDoll()
    {
        mSkinned =
            mMeshPool->CreateSkinnedModelFromFile("./Resources/Models/Fox.glb");
        if (mSkinned.submeshes.empty() || mSkinned.skeleton.JointCount() == 0)
        {
            std::cerr << "Failed to load Fox.glb for paper-doll preview\n";
            return;
        }

        mIdleClip = nullptr;
        for (const auto& clip : mSkinned.clips)
        {
            if (clip.name.find("Idle") != std::string::npos ||
                clip.name.find("Survey") != std::string::npos)
            {
                mIdleClip = &clip;
                break;
            }
        }
        if (!mIdleClip && !mSkinned.clips.empty())
            mIdleClip = &mSkinned.clips.front();

        auto windowSp = GetMainServiceProvider();
        mDollPreview  = std::make_unique<fra::UiModelPreview>(
            windowSp, *mTexturePool, glm::uvec2 { 512, 512 });

        auto& orbit       = mDollPreview->Orbit();
        orbit.enabled     = true;
        orbit.dragButton  = fra::MouseButton::Left;
        orbit.sensitivity = 0.35f;
        orbit.yawDeg      = 35.f;
        orbit.pitchDeg    = -12.f;
        orbit.distance    = 2.8f;
        orbit.autoRotate  = true;
        orbit.autoSpeed   = 18.f;
        orbit.target      = { 0.f, 0.55f, 0.f };
        mDollPreview->SetOrbit(orbit);

        rebuildDollScene();
        mRenderer->AddModelPreview(mDollPreview.get());
        mPortraitPending = true;
        mPortraitFrames  = 0;
    }

    void rebuildDollScene()
    {
        if (!mDollPreview || mSkinned.submeshes.empty())
            return;

        auto& scene = mDollPreview->PreviewScene();
        scene.Clear();

        const auto jointCount = mSkinned.skeleton.JointCount();
        const auto model      = glm::scale(glm::mat4(1.f), glm::vec3(0.02f));

        std::uint32_t entity = 1;
        for (const auto& part : mSkinned.submeshes)
        {
            fra::Scene::Instance inst {};
            inst.transform  = fra::SceneTransform::FromMatrix(model);
            inst.mesh       = part.mesh;
            inst.material   = part.material;
            inst.entityId   = entity++;
            inst.flags      = fra::kSceneInstanceFlagCastShadows |
                              fra::kSceneInstanceFlagSkinned;
            inst.boneOffset = 0;
            inst.boneCount  = jointCount;
            inst.mobility   = fra::Mobility::Dynamic;
            scene.Add(inst);
        }
    }

    void updateDollAnimation(float dt)
    {
        if (!mDollPreview || mSkinned.skeleton.JointCount() == 0)
            return;

        mAnimTime += dt;
        fra::LocalPose local;
        if (mIdleClip)
        {
            const float t = mIdleClip->duration > 0.f
                                ? std::fmod(mAnimTime, mIdleClip->duration)
                                : 0.f;
            local         = fra::SampleClip(mSkinned.skeleton, *mIdleClip, t);
        }
        else
            local = fra::RestLocalPose(mSkinned.skeleton);

        const auto bones = fra::PoseToSkinMatrices(mSkinned.skeleton, local);
        mRenderer->UploadBoneMatrices(bones);
        mDollPreview->SetFrameDelta(dt);
    }

    void maybeCapturePortrait()
    {
        if (!mPortraitPending || !mDollPreview)
            return;
        ++mPortraitFrames;
        // Wait for a few Recorded frames, then snapshot once (waitIdle).
        if (mPortraitFrames < 3)
            return;

        mPortraitPending = false;
        const auto snap =
            mDollPreview->CaptureSnapshot(*mTexturePool, { 96, 96 });
        if (snap.IsValid())
        {
            mPortrait = snap;
            std::cout << "Paper-doll portrait snapshot captured\n";
        }
    }

    void capturePortraitNow()
    {
        if (!mDollPreview)
            return;
        const auto snap =
            mDollPreview->CaptureSnapshot(*mTexturePool, { 96, 96 });
        if (snap.IsValid())
        {
            mPortrait        = snap;
            mPortraitPending = false;
            mConfirmMsg      = "Portrait updated from paper-doll.";
            mConfirmTimer    = 2.f;
        }
    }

    void rebuildScene()
    {
        mScene.Clear();
        fra::Scene::Instance ground {};
        ground.mesh      = mGroundMesh;
        ground.material  = mGroundMaterial;
        ground.mobility  = fra::Mobility::Static;
        ground.transform = fra::SceneTransform::FromMatrix(
            glm::translate(glm::mat4(1.f), glm::vec3(0.f, -1.f, 0.f)));
        mScene.Add(ground);
    }

    void drawGameUi(float dt)
    {
        auto& ui        = mRenderer->GetUiContext();
        ui.Style().font = mFont.Valid() ? &mFont : nullptr;

        const auto w = mFreyaOptions->width;
        const auto h = mFreyaOptions->height;
        ui.Begin(dt, { w, h });

        drawHud(ui);
        drawAbilityBar(ui);

        if (mConfirmTimer > 0.f)
        {
            mConfirmTimer -= dt;
            ui.BeginAnchor(fra::UiAnchor::Top, { -220.f, 80.f });
            if (ui.BeginPanel("confirm_toast", { 440.f, 64.f }))
            {
                ui.Label(mConfirmMsg, ui.Style().Var(fra::UiVar::FontSize));
                ui.EndPanel();
            }
            ui.EndAnchor();
        }

        switch (mScreen)
        {
            case UiScreen::Inventory:
                drawInventory(ui);
                break;
            case UiScreen::Dialogue:
                drawDialogue(ui);
                break;
            case UiScreen::Chat:
                drawChat(ui);
                break;
            case UiScreen::None:
            default:
                break;
        }

        ui.End();
    }

    void tryActivateAbility(std::size_t index)
    {
        if (index >= mAbilities.size())
            return;
        auto& a = mAbilities[index];
        if (a.remaining > 0.f)
            return;
        a.remaining = a.duration;
        mConfirmMsg = std::string("Cast ") + a.name + " — cooldown " +
                      std::to_string(static_cast<int>(a.duration + 0.5f)) + "s";
        mConfirmTimer = 1.6f;
        std::cout << "Ability: " << a.name << '\n';
    }

    void drawHud(fra::UiContext& ui)
    {
        ui.BeginAnchor(fra::UiAnchor::TopLeft, { 24.f, 24.f });
        {
            float widths[] = { 72.f, 230.f };
            ui.BeginColumns("hud_hp", 2, widths);
            ui.Image(mPortrait, glm::vec2 { 64.f, 64.f });
            ui.NextColumn();
            ui.Label("HP", ui.Style().Var(fra::UiVar::FontSizeSmall));
            ui.ProgressBar(mHpPulse, { 220.f, 18.f });
            ui.EndColumns();
        }
        ui.EndAnchor();

        ui.BeginAnchor(fra::UiAnchor::TopRight, { -420.f, 24.f });
        ui.Label("1-6 Skills  |  F1 Inv  |  F2 Dialog  |  F3 Chat",
                 ui.Style().Var(fra::UiVar::FontSizeSmall));
        ui.EndAnchor();
    }

    void drawAbilityBar(fra::UiContext& ui)
    {
        constexpr float kCell  = 68.f;
        constexpr float kGap   = 8.f;
        constexpr int   kCount = 6;
        const float     barW   = kCount * kCell + (kCount - 1) * kGap + 24.f;
        ui.BeginAnchor(fra::UiAnchor::Bottom, { -barW * 0.5f, -110.f });
        if (!ui.BeginPanel("ability_bar", { barW, kCell + 28.f }))
        {
            ui.EndAnchor();
            return;
        }

        ui.BeginGrid("ability_grid", kCount, { kCell, kCell }, kGap);
        for (std::size_t i = 0; i < mAbilities.size(); ++i)
        {
            auto&       a   = mAbilities[i];
            const float rem = a.duration > 0.f ? a.remaining / a.duration : 0.f;
            const auto  id  = std::string("ability_") + std::to_string(i);
            if (ui.AbilitySlot(id, a.icon, a.hotkey, rem, { kCell, kCell }))
                tryActivateAbility(i);

            if (ui.IsItemHovered() && ui.BeginTooltip("ability_tip", 0.2f))
            {
                ui.Label(a.name, ui.Style().Var(fra::UiVar::FontSize));
                ui.TextWrapped(a.description, 200.f);
                ui.EndTooltip();
            }
        }
        ui.EndGrid();
        ui.EndPanel();
        ui.EndAnchor();
    }

    void drawEquipSlot(fra::UiContext& ui, EquipSlot slot, float cell)
    {
        const auto idx   = static_cast<std::size_t>(slot);
        auto&      eq    = mEquip[idx];
        const auto id    = std::string("equip_") + std::to_string(idx);
        const auto icon  = eq ? eq->icon : fra::TextureHandle {};
        const int  stack = eq ? eq->stack : 0;

        if (ui.ItemSlot(id, icon, stack, false, { cell, cell }))
        {
            if (eq)
                std::cout << "Equip selected: " << eq->name << '\n';
        }

        if (eq && ui.IsItemHovered())
        {
            if (ui.BeginDragDropSource("equip_item",
                                       static_cast<std::uint64_t>(idx),
                                       eq->icon))
            {
            }
            if (ui.BeginTooltip("equip_tip"))
            {
                ui.Label(EquipSlotName(slot));
                ui.Label(eq->name);
                ui.EndTooltip();
            }
        }
        else if (!eq && ui.IsItemHovered() && ui.BeginTooltip("equip_empty"))
        {
            ui.Label(EquipSlotName(slot));
            ui.Label("(empty)", ui.Style().Var(fra::UiVar::FontSizeSmall));
            ui.EndTooltip();
        }

        std::uint64_t from = 0;
        if (ui.AcceptDragDropPayload("inv_item", &from))
        {
            const auto src = static_cast<std::size_t>(from);
            if (src < mSlots.size() && mSlots[src])
            {
                std::swap(mSlots[src], mEquip[idx]);
                std::cout << "Equipped to " << EquipSlotName(slot) << '\n';
            }
        }
        if (ui.AcceptDragDropPayload("equip_item", &from))
        {
            const auto src = static_cast<std::size_t>(from);
            if (src < mEquip.size() && src != idx)
                std::swap(mEquip[src], mEquip[idx]);
        }
    }

    void drawInventory(fra::UiContext& ui)
    {
        if (!ui.BeginModal("inventory", { 780.f, 520.f }))
            return;

        ui.Label("Inventory — paper-doll",
                 ui.Style().Var(fra::UiVar::FontSizeTitle));
        ui.Separator();

        float colW[] = { 300.f, 440.f };
        ui.BeginColumns("inv_cols", 2, colW);

        // Left: doll preview + equip ring
        {
            constexpr float kEquip = 56.f;
            ui.Label("Character", ui.Style().Var(fra::UiVar::FontSize));
            drawEquipSlot(ui, EquipSlot::Head, kEquip);
            drawEquipSlot(ui, EquipSlot::Chest, kEquip);

            if (mDollPreview)
            {
                ui.ModelPreview("paper_doll", mDollPreview->Texture(),
                                { 256.f, 256.f }, mDollPreview.get());
            }
            else
            {
                ui.Label("(preview unavailable)",
                         ui.Style().Var(fra::UiVar::FontSizeSmall));
            }

            drawEquipSlot(ui, EquipSlot::Hands, kEquip);
            drawEquipSlot(ui, EquipSlot::Legs, kEquip);
            drawEquipSlot(ui, EquipSlot::Feet, kEquip);
            drawEquipSlot(ui, EquipSlot::Weapon, kEquip);

            if (ui.Button("Tirar foto", { 140.f, 32.f }))
                capturePortraitNow();
            ui.Label("LMB drag orbit | auto-rotate",
                     ui.Style().Var(fra::UiVar::FontSizeSmall));
        }

        ui.NextColumn();

        // Right: bag grid
        {
            ui.Label("Bag", ui.Style().Var(fra::UiVar::FontSize));
            constexpr int   kCols = 5;
            constexpr float kCell = 72.f;
            constexpr float kGap  = 8.f;
            ui.BeginGrid("inv_grid", kCols, { kCell, kCell }, kGap);

            for (std::size_t i = 0; i < mSlots.size(); ++i)
            {
                const auto id   = std::string("slot_") + std::to_string(i);
                auto&      slot = mSlots[i];
                const fra::TextureHandle icon =
                    slot ? slot->icon : fra::TextureHandle {};
                const int stack = slot ? slot->stack : 0;

                if (ui.ItemSlot(id, icon, stack, false, { kCell, kCell }))
                {
                    if (slot)
                        std::cout << "Selected: " << slot->name << '\n';
                }

                if (slot && ui.IsItemHovered())
                {
                    if (ui.BeginDragDropSource("inv_item",
                                               static_cast<std::uint64_t>(i),
                                               slot->icon))
                    {
                    }
                    if (ui.BeginTooltip("tip"))
                    {
                        ui.Label(slot->name);
                        ui.Label(std::string("x") + std::to_string(slot->stack),
                                 ui.Style().Var(fra::UiVar::FontSizeSmall));
                        ui.EndTooltip();
                    }
                }

                if (slot)
                {
                    const auto ctxId = std::string("ctx_") + std::to_string(i);
                    if (ui.BeginPopupContextItem(ctxId))
                    {
                        if (ui.Button("Use", { 140.f, 28.f }))
                        {
                            std::cout << "Use " << slot->name << '\n';
                            if (slot->stack > 1)
                                --slot->stack;
                            else
                                slot.reset();
                        }
                        if (slot && ui.Button("Discard", { 140.f, 28.f }))
                        {
                            std::cout << "Discard " << slot->name << '\n';
                            slot.reset();
                        }
                        ui.EndPopup();
                    }
                }

                std::uint64_t from = 0;
                if (ui.AcceptDragDropPayload("inv_item", &from))
                {
                    const auto src = static_cast<std::size_t>(from);
                    if (src < mSlots.size() && src != i)
                        std::swap(mSlots[src], mSlots[i]);
                }
                if (ui.AcceptDragDropPayload("equip_item", &from))
                {
                    const auto src = static_cast<std::size_t>(from);
                    if (src < mEquip.size())
                        std::swap(mEquip[src], mSlots[i]);
                }
            }

            ui.EndGrid();
        }

        ui.EndColumns();

        if (ui.Button("Close", { 160.f, 36.f }))
            mScreen = UiScreen::None;

        ui.EndModal();
    }

    void drawDialogue(fra::UiContext& ui)
    {
        ui.BeginAnchor(fra::UiAnchor::Bottom, { -480.f, -220.f });
        if (!ui.BeginPanel("dialog", { 960.f, 200.f }))
        {
            ui.EndAnchor();
            return;
        }

        float widths[] = { 140.f, 780.f };
        ui.BeginColumns("dlg_cols", 2, widths);
        ui.Image(mPortrait, glm::vec2 { 120.f, 120.f });
        ui.NextColumn();

        static const char* lines[] = {
            "Traveler: The road north is blocked by mist.",
            "Traveler: Will you help clear the path?",
        };
        ui.Label("Traveler", ui.Style().Var(fra::UiVar::FontSizeTitle));
        ui.TextWrapped(lines[mDialogueLine % 2], 740.f);

        if (mDialogueLine == 0)
        {
            if (ui.Button("Continue", { 160.f, 32.f }))
                mDialogueLine = 1;
        }
        else
        {
            if (ui.Selectable("I will help.", mDialogueChoice == 0))
                mDialogueChoice = 0;
            if (ui.Selectable("Not today.", mDialogueChoice == 1))
                mDialogueChoice = 1;
            if (ui.Button("Confirm", { 160.f, 32.f }))
            {
                const bool help = mDialogueChoice == 0;
                mConfirmMsg     = help ? "You agreed to help the traveler."
                                       : "You declined. The mist remains.";
                mConfirmTimer   = 3.5f;
                std::cout << "Dialogue choice: " << (help ? "help" : "decline")
                          << '\n';
                mScreen         = UiScreen::None;
                mDialogueLine   = 0;
                mDialogueChoice = 0;
            }
        }

        ui.EndColumns();
        ui.EndPanel();
        ui.EndAnchor();
    }

    void drawChat(fra::UiContext& ui)
    {
        ui.BeginAnchor(fra::UiAnchor::BottomRight, { -420.f, -360.f });
        if (!ui.BeginPanel("chat", { 400.f, 340.f }))
        {
            ui.EndAnchor();
            return;
        }

        ui.Label("Chat", ui.Style().Var(fra::UiVar::FontSizeTitle));
        constexpr float kItemH = 22.f;
        ui.BeginList("chat_list", { 360.f, 220.f },
                     static_cast<int>(mChatLog.size()), kItemH);
        if (mChatScrollBottom)
        {
            ui.SetScrollHereY(1.f);
            mChatScrollBottom = false;
        }
        for (std::size_t i = 0; i < mChatLog.size(); ++i)
            ui.ListItem(static_cast<int>(i), mChatLog[i], false);
        ui.EndList();

        if (mChatFocusPending)
        {
            ui.FocusTextInput("chat_input", mChatInput);
            mChatFocusPending = false;
        }
        if (ui.TextInput("chat_input", mChatInput, 128, { 360.f, 32.f }))
        {
            if (!mChatInput.empty())
            {
                mChatLog.push_back(std::string("You: ") + mChatInput);
                mChatInput.clear();
                mChatScrollBottom = true;
            }
        }

        ui.EndPanel();
        ui.EndAnchor();
    }

    fra::Ref<fra::MaterialPool> mMaterialPool;
    fra::Ref<fra::TexturePool>  mTexturePool;
    fra::Ref<fra::MeshPool>     mMeshPool;
    fra::Ref<fra::LightService> mLightService;
    fra::Ref<fra::FreyaOptions> mFreyaOptions;

    FreyaExamples::FlyCam       mMainCam;
    FreyaExamples::DebugOverlay mOverlay;
    fra::Scene                  mScene;
    fra::MeshHandle             mGroundMesh {};
    fra::MaterialHandle         mGroundMaterial {};

    fra::FontAtlas     mFont;
    fra::TextureHandle mIconSword {};
    fra::TextureHandle mIconPotion {};
    fra::TextureHandle mIconGem {};
    fra::TextureHandle mPortrait {};
    fra::TextureHandle mIconFire {};
    fra::TextureHandle mIconIce {};
    fra::TextureHandle mIconHeal {};
    fra::TextureHandle mIconShield {};
    fra::TextureHandle mIconDash {};
    fra::TextureHandle mIconStun {};

    std::unique_ptr<fra::UiModelPreview> mDollPreview;
    fra::SkinnedModel                    mSkinned;
    const fra::AnimationClip*            mIdleClip        = nullptr;
    float                                mAnimTime        = 0.f;
    bool                                 mPortraitPending = false;
    int                                  mPortraitFrames  = 0;

    UiScreen                               mScreen = UiScreen::None;
    std::array<std::optional<InvItem>, 20> mSlots {};
    std::array<std::optional<InvItem>,
               static_cast<std::size_t>(EquipSlot::Count)>
                           mEquip {};
    std::array<Ability, 6> mAbilities {};
    float                  mHpPulse = 1.f;
    float                  mTime    = 0.f;

    int                      mDialogueLine   = 0;
    int                      mDialogueChoice = 0;
    std::string              mConfirmMsg;
    float                    mConfirmTimer = 0.f;
    std::vector<std::string> mChatLog;
    std::string              mChatInput;
    bool                     mChatScrollBottom = false;
    bool                     mChatFocusPending = false;
};

int main(int, const char**)
{
    return fra::RunApp<MainApp>(
        [](fra::FreyaOptionsBuilder& freyaOptions) {
            freyaOptions
                .SetTitle(
                    "GameUiDemo — Freya UI [1-6 skills | F1/F2/F3 | RMB+WASD]")
                .SetWidth(1920)
                .SetHeight(1080)
                .SetVSync(false)
                .SetSampleCount(4)
                .WithReverseZ()
                .SetFullscreen(false)
                .SetEnableSsao(false)
                .SetEnableTaa(false)
                .SetEnableBloom(false);
        },
        [](skr::LoggingExtension& l) {
            FreyaExamples::ConfigureLogging(l, "GameUiDemo.log");
        });
}
