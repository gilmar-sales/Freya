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
#include <span>
#include <string>
#include <string_view>
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
        Showcase  = 4,
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
        mOverlay.SetCullDumpExampleName("GameUiShowcase");

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
                if (event.key == fra::KeyCode::F4)
                {
                    mScreen = mScreen == UiScreen::Showcase
                                  ? UiScreen::None
                                  : UiScreen::Showcase;
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

        mShowSlots.fill(std::nullopt);
        mShowSlots[0] = InvItem { "Iron Sword", mIconSword, 1 };
        mShowSlots[1] = InvItem { "Health Potion", mIconPotion, 5 };
        mShowSlots[2] = InvItem { "Sapphire", mIconGem, 12 };
        mShowSlots[4] = InvItem { "Ember", mIconFire, 2 };

        mChatLog = {
            "[System] Welcome to GameUiShowcase.",
            "[Hint] Keys 1-6 cast abilities. F1 inventario, F2 dialogo, F3 "
            "chat, F4 showcase.",
            "[Hint] F1 paper-doll: drag orbit, equip slots, Tirar foto.",
            "[Hint] F4 hub: basics, inputs, layout, windows, game widgets.",
            "[Hint] RMB+WASD to look/move; Esc closes UI.",
        };
        mChatInput.clear();

        mShopNames = { "Potion",     "Ether",    "Antidote", "Iron Sword",
                       "Oak Shield", "Sapphire", "Ember",    "Frost Shard",
                       "Herb",       "Bomb",     "Rope",     "Torch",
                       "Key",        "Map",      "Compass",  "Boots",
                       "Gloves",     "Helm",     "Bow",      "Arrow x10",
                       "Bread",      "Cheese",   "Wine" };
        mQuestLog.reserve(40);
        for (int i = 0; i < 40; ++i)
            mQuestLog.push_back("Quest " + std::to_string(i + 1) +
                                " — rumor from the crossroads");

        mGroundMesh = FreyaExamples::CreateGroundPlane(
            *mMeshPool, 40.0f, glm::vec3(0.35f, 0.36f, 0.38f));
        mGroundMaterial = mMaterialPool->Create({});

        mLightService->AddLight(fra::MakeDirectionalLight(
            glm::vec3(-0.3f, -1.0f, -0.2f), glm::vec3(1.0f, 0.96f, 0.9f),
            1.8f));

        setupPaperDoll();
        rebuildScene();

        std::cout
            << "GameUiShowcase — native Freya UI (not ImGui)\n"
            << "  1-6 Abilities | F1 Inventory (paper-doll) | F2 Dialogue | "
               "F3 Chat | F4 Showcase | Esc\n"
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
            mPortraitPending || mScreen == UiScreen::Inventory ||
            mScreen == UiScreen::Showcase;
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
            case UiScreen::Showcase:
                drawShowcase(ui);
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

        ui.BeginAnchor(fra::UiAnchor::TopRight, { -560.f, 24.f });
        ui.Label("1-6 Skills | F1 Inv | F2 Dialog | F3 Chat | F4 Showcase",
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
        if (!ui.BeginModal("inventory", { 780.f, 600.f }))
            return;

        ui.Label("Inventory — paper-doll",
                 ui.Style().Var(fra::UiVar::FontSizeTitle));
        ui.Separator();

        // Doll column (~740px) exceeds the modal: scroll both columns.
        ui.BeginScrollView("inv_scroll", { 756.f, 480.f }, 760.f);

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
        ui.EndScrollView();

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

    // --- Showcase hub (F4): every basic + complex widget ---
    void drawShowcase(fra::UiContext& ui)
    {
        if (ui.BeginDrawer("show_nav", fra::UiAnchor::Left, 250.f))
        {
            ui.Heading("Navigate");
            ui.TextWrapped("F4 toggles this hub. Esc closes it. "
                           "Tabs live in the floating window.",
                           210.f);
            ui.Separator();
            if (ui.Button("Toast!", { 200.f, 32.f }))
                ui.ShowToast("Hello from GameUiShowcase!", 2.5f);
            if (ui.Button("Browse files...", { 200.f, 32.f }))
                ui.OpenFileDialog("show_file", ".");
            if (ui.Button("Reset wizard", { 200.f, 32.f }))
                mWizardStep = 0;
            ui.Checkbox("Companion window", &mSecondWindowOpen);
            ui.Checkbox("Right inspector", &mInspectorOpen);
            if (!mInspectorOpen)
                ui.OpenDrawer("show_inspector", false);
            else
                ui.OpenDrawer("show_inspector", true);
            ui.Separator();
            ui.Bullet("Drag the hub by its title bar");
            ui.Bullet("Resize via the bottom-right grip");
            ui.Bullet("Collapse with the chevron");
            ui.EndDrawer();
        }

        if (ui.BeginDrawer("show_inspector", fra::UiAnchor::Right, 300.f))
        {
            ui.Heading("Inspector");
            const auto last = ui.LastItemRect();
            ui.Label(std::string("Hover: ") +
                     (ui.IsItemHovered() ? "yes" : "no"));
            ui.Label(std::string("Active: ") +
                     (ui.IsItemActive() ? "yes" : "no"));
            ui.Label(std::string("Clicked: ") +
                     (ui.IsItemClicked() ? "yes" : "no"));
            ui.Label("Last rect: " + std::to_string(int(last.x)) + "," +
                     std::to_string(int(last.y)) + " " +
                     std::to_string(int(last.w)) + "x" +
                     std::to_string(int(last.h)));
            ui.Separator();
            ui.Label("Clipboard:", ui.Style().Var(fra::UiVar::FontSizeSmall));
            ui.TextWrapped(ui.Clipboard().empty() ? "(empty)" : ui.Clipboard(),
                           250.f);
            if (ui.Button("Clear", { 120.f, 28.f }))
                ui.SetClipboard("");
            ui.Separator();
            const auto wr = ui.WindowRect("showcase_main");
            ui.Label("Hub rect: " + std::to_string(int(wr.w)) + "x" +
                     std::to_string(int(wr.h)));
            ui.Label(std::string("Drawer L: ") +
                     (ui.IsDrawerOpen("show_nav") ? "open" : "closed"));
            ui.EndDrawer();
        }

        fra::UiWindowOpts hubOpts {};
        hubOpts.defaultPos  = { 350.f, 150.f };
        hubOpts.defaultSize = { 1180.f, 720.f };
        hubOpts.closable    = true;
        if (ui.BeginWindow("showcase_main", "Game UI Showcase — F4",
                           { 1180.f, 720.f }, hubOpts))
        {
            ui.BeginTabBar("show_tabs");
            float tabW[] = { 1.f, 1.f, 1.f, 1.f, 1.f };
            ui.BeginRow("show_tabrow", std::span<const float>(tabW, 5), 6.f);
            const bool tBasics = ui.Tab("Basics");
            ui.NextCell();
            const bool tInputs = ui.Tab("Inputs");
            ui.NextCell();
            const bool tLayout = ui.Tab("Layout");
            ui.NextCell();
            const bool tWindows = ui.Tab("Windows");
            ui.NextCell();
            const bool tGame = ui.Tab("Game");
            ui.EndRow();
            ui.EndTabBar();
            ui.Separator();

            if (ui.BeginSwitcher("show_pages", { 1140.f, 590.f }))
            {
                if (tBasics)
                    drawShowBasics(ui);
                else if (tInputs)
                    drawShowInputs(ui);
                else if (tLayout)
                    drawShowLayout(ui);
                else if (tWindows)
                    drawShowWindows(ui);
                else if (tGame)
                    drawShowGame(ui);
                else
                    drawShowBasics(ui);
            }
            ui.EndSwitcher();
        }
        ui.EndWindow();
        if (!ui.IsWindowOpen("showcase_main") && mScreen == UiScreen::Showcase)
            mScreen = UiScreen::None;

        if (mSecondWindowOpen)
        {
            fra::UiWindowOpts opts {};
            opts.defaultPos  = { 60.f, 420.f };
            opts.defaultSize = { 320.f, 260.f };
            opts.closable    = true;
            if (ui.BeginWindow("show_companion", "Companion", { 320.f, 260.f },
                               opts))
            {
                ui.Label("Floating window demo",
                         ui.Style().Var(fra::UiVar::FontSize));
                ui.TextWrapped("Drag by the title bar, resize with the "
                               "grip, collapse with the chevron, close "
                               "with x.",
                               280.f);
                ui.Separator();
                ui.Spinner("companion_spin", { 28.f, 28.f });
                ui.ProgressBar(mHpPulse, { 260.f, 14.f });
                if (ui.Button("Uncheck hub box", { 200.f, 30.f }))
                    mSecondWindowOpen = false;
            }
            ui.EndWindow();
            if (!ui.IsWindowOpen("show_companion"))
                mSecondWindowOpen = false;
        }

        std::string           picked;
        fra::UiFileDialogOpts fileOpts {};
        fileOpts.directory = ".";
        if (ui.FileDialog("show_file", "Open asset", fileOpts, &picked))
        {
            mFilePath = picked;
            ui.ShowToast("Picked: " + mFilePath, 3.f);
            std::cout << "FileDialog picked: " << mFilePath << '\n';
        }
    }

    void drawShowBasics(fra::UiContext& ui)
    {
        ui.BeginScrollView("basics_scroll", { 1140.f, 590.f }, 1050.f);
        ui.Heading("Display widgets");
        ui.Label("Plain label at default size.");
        ui.LabelColored("Colored label — quest updated!",
                        { 0.4f, 0.9f, 0.5f, 1.f });
        ui.LabelColored("Colored label — low health!",
                        { 0.95f, 0.35f, 0.3f, 1.f });
        ui.TextWrapped("Wrapped paragraph: the northern road is blocked by "
                       "mist, and the showcase hub demonstrates every basic "
                       "display widget (labels, bullets, bars, headers, "
                       "spinners, toasts) in one scrollable page.",
                       1050.f);
        ui.Bullet("First bullet — movement with WASD");
        ui.Bullet("Second bullet — abilities on 1-6");
        ui.Bullet("Third bullet — Esc closes panels");
        ui.Separator();

        ui.Label("Bars", ui.Style().Var(fra::UiVar::FontSize));
        ui.Label("HP", ui.Style().Var(fra::UiVar::FontSizeSmall));
        ui.ProgressBar(mHpPulse, { 500.f, 16.f });
        ui.Label("MP", ui.Style().Var(fra::UiVar::FontSizeSmall));
        ui.ProgressBar(0.72f, { 500.f, 16.f });
        ui.PushStyleColor(fra::UiCol::ProgressFill, { 0.9f, 0.75f, 0.2f, 1.f });
        ui.Label("XP (tinted via PushStyleColor)", 0.f);
        ui.ProgressBar(mXp01, { 500.f, 16.f });
        ui.PopStyleColor();
        ui.Separator();

        ui.Label("Header + spinner + toast",
                 ui.Style().Var(fra::UiVar::FontSize));
        if (ui.CollapsingHeader("basics_lore", "Lore (collapsible)", true))
        {
            ui.TextWrapped("The fox spirit idles in the paper-doll while "
                           "you read. Collapse this header to hide the tale.",
                           1050.f);
        }
        if (ui.CollapsingHeader("basics_credits", "Credits (collapsible)",
                                false))
        {
            ui.Label("Freya engine — native game UI, no ImGui.");
        }
        float spinRow[] = { 40.f, 400.f, 200.f };
        ui.BeginColumns("basics_spin", 3, spinRow);
        ui.Spinner("basics_spinner", { 28.f, 28.f });
        ui.NextColumn();
        ui.Label("Spinner animates with ui time.");
        ui.NextColumn();
        if (ui.Button("Show toast", { 160.f, 32.f }))
            ui.ShowToast("Basics toast — fades automatically.", 2.5f);
        ui.EndColumns();
        ui.Separator();

        ui.Label("Images", ui.Style().Var(fra::UiVar::FontSize));
        float imgRow[] = { 120.f, 300.f, 500.f };
        ui.BeginColumns("basics_imgs", 3, imgRow);
        ui.Image(mPortrait, glm::vec2 { 96.f, 96.f });
        ui.NextColumn();
        fra::UiImageOpts cover {};
        cover.fit = fra::UiImageFit::Cover;
        ui.Image(mPortrait, glm::vec2 { 96.f, 96.f }, cover);
        ui.Label("Cover-fit portrait (same handle).",
                 ui.Style().Var(fra::UiVar::FontSizeSmall));
        ui.NextColumn();
        if (ui.BeginPanel("basics_bg", { 420.f, 96.f }))
        {
            ui.Background(mPortrait, fra::UiImageFit::Cover,
                          { 1.f, 1.f, 1.f, 0.55f });
            ui.Label("Panel with Background() tint.");
            ui.EndPanel();
        }
        ui.EndColumns();
        ui.Separator();

        ui.Label("Selectable difficulty", ui.Style().Var(fra::UiVar::FontSize));
        static const char* kDiffs[] = { "Story", "Normal", "Nightmare" };
        for (int i = 0; i < 3; ++i)
        {
            if (ui.Selectable(kDiffs[i], mDifficulty == i, { 300.f, 30.f }))
                mDifficulty = i;
        }
        ui.EndScrollView();
    }

    void drawShowInputs(fra::UiContext& ui)
    {
        ui.BeginScrollView("inputs_scroll", { 1140.f, 590.f }, 1000.f);
        ui.Heading("Input widgets");

        float btnRow[] = { 180.f, 180.f, 300.f };
        ui.BeginColumns("inputs_btns", 3, btnRow);
        if (ui.Button("Primary", { 160.f, 36.f }))
            ui.ShowToast("Primary pressed.", 1.5f);
        ui.NextColumn();
        ui.BeginDisabled(mInputsLocked);
        if (ui.Button("Maybe disabled", { 160.f, 36.f }))
            ui.ShowToast("Disabled button fired?!", 1.5f);
        ui.EndDisabled();
        ui.NextColumn();
        ui.Checkbox("Lock middle button", &mInputsLocked);
        ui.EndColumns();
        ui.Separator();

        ui.Checkbox("Music enabled", &mCheckMusic);
        ui.Checkbox("Shadows enabled", &mCheckShadows);
        ui.ToggleSwitch("VSync", &mToggleVsync);
        ui.ToggleSwitch("Auto-rotate doll", &mToggleRotate);
        if (mDollPreview)
        {
            auto orbit       = mDollPreview->Orbit();
            orbit.autoRotate = mToggleRotate;
            mDollPreview->SetOrbit(orbit);
        }
        ui.Separator();

        ui.SliderFloat("Volume", &mVolume01, 0.f, 1.f, { 320.f, 24.f });
        ui.SliderInt("Brightness", &mBrightness, 0, 100, { 320.f, 24.f });
        ui.Label("Party size", ui.Style().Var(fra::UiVar::FontSizeSmall));
        ui.SpinBox("party_size", &mPartySize, 1, 6, 1, { 320.f, 32.f });
        ui.Separator();

        ui.Label("Radio group — starter relic",
                 ui.Style().Var(fra::UiVar::FontSizeSmall));
        static const char* kRelics[] = { "Ember", "Frost", "Gale" };
        for (int i = 0; i < 3; ++i)
        {
            if (ui.RadioButton(kRelics[i], mRelic == i, { 320.f, 26.f }))
                mRelic = i;
        }
        ui.Separator();

        std::array<std::string_view, 4> classes = { "Knight", "Ranger",
                                                    "Scholar", "Fox" };
        ui.ComboBox("input_class", std::span<const std::string_view>(classes),
                    &mClassIdx, { 320.f, 32.f });
        ui.Label(std::string("Selected class: ") +
                 std::string(classes[std::size_t(mClassIdx)]));
        ui.Separator();

        ui.Label("Tint (ColorEdit)", ui.Style().Var(fra::UiVar::FontSizeSmall));
        if (ui.ColorEdit("input_tint", &mTintRgb))
            std::cout << "Tint changed\n";
        ui.Separator();

        ui.Label("Search + text", ui.Style().Var(fra::UiVar::FontSizeSmall));
        ui.SearchBox("input_search", mSearchBuf, 64, { 320.f, 32.f });
        ui.Label("Hero name (Enter submits):",
                 ui.Style().Var(fra::UiVar::FontSizeSmall));
        float nameRow[] = { 320.f, 160.f };
        ui.BeginColumns("inputs_name", 2, nameRow);
        if (ui.TextInput("input_name", mHeroName, 32, { 300.f, 32.f }))
        {
            ui.ShowToast("Welcome, " +
                             (mHeroName.empty() ? "Nameless" : mHeroName) + "!",
                         2.f);
        }
        ui.NextColumn();
        if (ui.Button("Focus name", { 140.f, 32.f }))
            ui.FocusTextInput("input_name", mHeroName);
        ui.EndColumns();
        ui.Separator();

        ui.Label("Selectable row", ui.Style().Var(fra::UiVar::FontSizeSmall));
        for (int i = 0; i < 3; ++i)
        {
            const std::string label = "Slot " + std::to_string(i + 1);
            if (ui.Selectable(label, mInputSel == i, { 320.f, 30.f }))
                mInputSel = i;
        }
        ui.EndScrollView();
    }

    void drawShowLayout(fra::UiContext& ui)
    {
        ui.BeginScrollView("layout_scroll", { 1140.f, 590.f }, 1250.f);
        ui.Heading("Layout containers");

        ui.Label("BeginRow (weights 2/1/1)",
                 ui.Style().Var(fra::UiVar::FontSize));
        float rowW[] = { 2.f, 1.f, 1.f };
        ui.BeginRow("layout_row", std::span<const float>(rowW, 3), 8.f);
        ui.Label("Wide cell — quest tracker lives here.");
        ui.NextCell();
        if (ui.Button("A", { 120.f, 32.f }))
            ui.ShowToast("Cell A", 1.f);
        ui.NextCell();
        if (ui.Button("B", { 120.f, 32.f }))
            ui.ShowToast("Cell B", 1.f);
        ui.EndRow();
        ui.Separator();

        ui.Label("BeginMargin (pad 16)", ui.Style().Var(fra::UiVar::FontSize));
        if (ui.BeginPanel("layout_margin_panel", { 1060.f, 110.f }))
        {
            if (ui.BeginMargin("layout_margin", 16.f))
            {
                ui.Label("Padded content — 16px on every side.");
                ui.TextWrapped("Margins keep dense HUD blocks breathable "
                               "without hand-tuned offsets.",
                               900.f);
                ui.EndMargin();
            }
            ui.EndPanel();
        }
        ui.Separator();

        ui.Label("BeginCenter", ui.Style().Var(fra::UiVar::FontSize));
        if (ui.BeginPanel("layout_center_panel", { 1060.f, 110.f }))
        {
            // Panel content fills the whole panel; center a button box.
            if (ui.BeginCenter("layout_center", { 220.f, 48.f }))
            {
                if (ui.Button("Centered", { 200.f, 36.f }))
                    ui.ShowToast("Bullseye.", 1.2f);
                ui.EndCenter();
            }
            ui.EndPanel();
        }
        ui.Separator();

        ui.Label("BeginVStack (gap 4 vs 16)",
                 ui.Style().Var(fra::UiVar::FontSize));
        float stackCols[] = { 500.f, 500.f };
        ui.BeginColumns("layout_stacks", 2, stackCols);
        if (ui.BeginVStack("layout_tight", 4.f))
        {
            ui.Label("Tight stack");
            ui.Label("line two");
            ui.Label("line three");
            ui.EndVStack();
        }
        ui.NextColumn();
        if (ui.BeginVStack("layout_loose", 16.f))
        {
            ui.Label("Loose stack");
            ui.Label("line two");
            ui.Label("line three");
            ui.EndVStack();
        }
        ui.EndColumns();
        ui.Separator();

        ui.Label("BeginColumns + BeginGrid",
                 ui.Style().Var(fra::UiVar::FontSize));
        float twoCol[] = { 500.f, 500.f };
        ui.BeginColumns("layout_cols", 2, twoCol);
        ui.Label("Left column text.");
        ui.NextColumn();
        ui.Label("Right column text.");
        ui.EndColumns();
        ui.BeginGrid("layout_grid", 4, { 72.f, 72.f }, 8.f);
        for (int i = 0; i < 8; ++i)
        {
            const auto id = std::string("layout_cell_") + std::to_string(i);
            ui.ItemSlot(id, i % 2 ? mIconGem : mIconPotion, 0, false,
                        { 72.f, 72.f });
        }
        ui.EndGrid();
        ui.Separator();

        ui.Label("Style stack (Push/Pop)",
                 ui.Style().Var(fra::UiVar::FontSize));
        ui.PushStyleColor(fra::UiCol::Button, { 0.2f, 0.5f, 0.25f, 1.f });
        ui.PushStyleVar(fra::UiVar::FontSize, 22.f);
        if (ui.Button("Styled", { 200.f, 40.f }))
            ui.ShowToast("Styled button.", 1.2f);
        ui.PopStyleVar();
        ui.PopStyleColor();
        ui.Separator();

        ui.Label("Click gestures", ui.Style().Var(fra::UiVar::FontSize));
        if (ui.Button("Double-click / long-press me", { 300.f, 36.f }))
            mGestureMsg = "Single click.";
        if (ui.IsItemDoubleClicked())
            mGestureMsg = "Double-click detected!";
        if (ui.IsItemLongPressed(0.6f))
            mGestureMsg = "Long press detected!";
        ui.Label(mGestureMsg);
        ui.Separator();

        ui.Label("Clipboard", ui.Style().Var(fra::UiVar::FontSize));
        float clipRow[] = { 320.f, 140.f, 140.f };
        ui.BeginColumns("layout_clip", 3, clipRow);
        ui.TextInput("layout_clip_text", mClipText, 64, { 300.f, 32.f });
        ui.NextColumn();
        if (ui.Button("Copy", { 120.f, 32.f }))
            ui.SetClipboard(mClipText);
        ui.NextColumn();
        if (ui.Button("Paste", { 120.f, 32.f }))
            mClipText = ui.Clipboard();
        ui.EndColumns();
        ui.Separator();

        ui.Label("Disabled block", ui.Style().Var(fra::UiVar::FontSize));
        ui.Checkbox("Disable controls", &mLayoutLocked);
        ui.BeginDisabled(mLayoutLocked);
        ui.SliderFloat("Locked slider", &mVolume01, 0.f, 1.f, { 320.f, 24.f });
        if (ui.Button("Locked button", { 200.f, 34.f }))
            ui.ShowToast("Should not fire while locked.", 1.f);
        ui.EndDisabled();
        ui.EndScrollView();
    }

    void drawShowWindows(fra::UiContext& ui)
    {
        ui.BeginScrollView("windows_scroll", { 1140.f, 590.f }, 1250.f);
        ui.Heading("Windows & navigation");

        ui.Checkbox("Show companion window", &mSecondWindowOpen);
        ui.Label("Hub is itself a BeginWindow: drag / resize / collapse / "
                 "close all work. State persists in WidgetState.");
        ui.Separator();

        ui.Label("BeginSwitcher", ui.Style().Var(fra::UiVar::FontSize));
        float swRow[] = { 120.f, 200.f, 120.f };
        ui.BeginColumns("switcher_nav", 3, swRow);
        if (ui.Button("< Prev", { 110.f, 30.f }))
            mSwitcherPage = (mSwitcherPage + 2) % 3;
        ui.NextColumn();
        ui.Label("Page " + std::to_string(mSwitcherPage + 1) + " / 3");
        ui.NextColumn();
        if (ui.Button("Next >", { 110.f, 30.f }))
            mSwitcherPage = (mSwitcherPage + 1) % 3;
        ui.EndColumns();
        if (ui.BeginSwitcher("demo_switcher", { 500.f, 84.f }))
        {
            static const char* kPages[] = { "Supplies — potions & rope",
                                            "Armory — swords & shields",
                                            "Maps — compass & keys" };
            ui.Label(kPages[mSwitcherPage % 3]);
            ui.TextWrapped("Switcher is a fixed-size stacked container; "
                           "only the active page draws.",
                           460.f);
        }
        ui.EndSwitcher();
        ui.Separator();

        ui.Label("BeginWizard", ui.Style().Var(fra::UiVar::FontSize));
        std::array<std::string_view, 3> steps = { "Hero", "Loadout", "Ready" };
        if (ui.BeginWizard("demo_wizard",
                           std::span<const std::string_view>(steps),
                           mWizardStep, { 700.f, 240.f }))
        {
            if (mWizardStep == 0)
            {
                ui.Label("Name your hero:");
                ui.TextInput("wiz_name", mHeroName, 32, { 320.f, 32.f });
                std::array<std::string_view, 4> classes = { "Knight", "Ranger",
                                                            "Scholar", "Fox" };
                ui.ComboBox("wiz_class",
                            std::span<const std::string_view>(classes),
                            &mClassIdx, { 320.f, 32.f });
            }
            else if (mWizardStep == 1)
            {
                ui.Label("Party size",
                         ui.Style().Var(fra::UiVar::FontSizeSmall));
                ui.SpinBox("wiz_party", &mPartySize, 1, 6, 1, { 320.f, 32.f });
                ui.ToggleSwitch("Bring torch", &mCheckMusic);
            }
            else
            {
                ui.Label(
                    "Ready to depart, " +
                    (mHeroName.empty() ? std::string("Nameless") : mHeroName) +
                    "!");
                ui.Label("Party size: " + std::to_string(mPartySize));
            }
            const int nav =
                ui.WizardNav("demo_wiznav", mWizardStep, 3, { 700.f, 40.f });
            if (nav < 0 && mWizardStep > 0)
                --mWizardStep;
            else if (nav > 0 && mWizardStep < 2)
                ++mWizardStep;
            else if (nav > 0 && mWizardStep == 2)
            {
                ui.ShowToast("Quest accepted!", 2.5f);
                mConfirmMsg   = "Wizard finished — quest accepted.";
                mConfirmTimer = 2.5f;
            }
            ui.EndWizard();
        }
        ui.Separator();

        ui.Label("Drawers", ui.Style().Var(fra::UiVar::FontSize));
        float drRow[] = { 220.f, 220.f, 400.f };
        ui.BeginColumns("drawer_btns", 3, drRow);
        if (ui.Button("Toggle left", { 200.f, 32.f }))
            ui.OpenDrawer("show_nav", !ui.IsDrawerOpen("show_nav"));
        ui.NextColumn();
        if (ui.Button("Toggle right", { 200.f, 32.f }))
        {
            mInspectorOpen = !mInspectorOpen;
            ui.OpenDrawer("show_inspector", mInspectorOpen);
        }
        ui.NextColumn();
        ui.Label(std::string("Left: ") +
                 (ui.IsDrawerOpen("show_nav") ? "open" : "closed") +
                 " | Right: " +
                 (ui.IsDrawerOpen("show_inspector") ? "open" : "closed"));
        ui.EndColumns();
        ui.Separator();

        ui.Label("Paginate (shop)", ui.Style().Var(fra::UiVar::FontSize));
        const int     itemCount = static_cast<int>(mShopNames.size());
        constexpr int kPage     = 6;
        ui.Paginate("shop_pager", itemCount, kPage, &mShopPage);
        int first = 0;
        int count = 0;
        fra::UiContext::PageRange(itemCount, kPage, mShopPage, &first, &count);
        ui.Label("Showing " + std::to_string(first + 1) + "-" +
                 std::to_string(first + count) + " of " +
                 std::to_string(itemCount));
        ui.BeginGrid("shop_grid", 6, { 56.f, 56.f }, 8.f);
        for (int i = 0; i < count; ++i)
        {
            const int          idx = first + i;
            const auto         id  = std::string("shop_") + std::to_string(idx);
            fra::TextureHandle icon =
                idx % 3 == 0 ? mIconPotion
                             : (idx % 3 == 1 ? mIconSword : mIconGem);
            if (ui.ItemSlot(id, icon, 0, false, { 56.f, 56.f }))
                ui.ShowToast("Browsing " + mShopNames[std::size_t(idx)] + ".",
                             1.2f);
            if (ui.IsItemHovered() && ui.BeginTooltip("shop_tip"))
            {
                ui.Label(mShopNames[std::size_t(idx)]);
                ui.Label(std::to_string(10 + idx * 5) + " gold",
                         ui.Style().Var(fra::UiVar::FontSizeSmall));
                ui.EndTooltip();
            }
        }
        ui.EndGrid();
        ui.Separator();

        ui.Label("FileDialog", ui.Style().Var(fra::UiVar::FontSize));
        float fRow[] = { 220.f, 600.f };
        ui.BeginColumns("file_row", 2, fRow);
        if (ui.Button("Browse...", { 200.f, 34.f }))
            ui.OpenFileDialog("show_file", ".");
        ui.NextColumn();
        ui.Label(mFilePath.empty() ? "No file picked yet." : mFilePath);
        ui.EndColumns();
        ui.EndScrollView();
    }

    void drawShowGame(fra::UiContext& ui)
    {
        ui.BeginScrollView("game_scroll", { 1140.f, 590.f }, 1100.f);
        ui.Heading("Game widgets");

        ui.Label("Drag & drop stash (type: show_item)",
                 ui.Style().Var(fra::UiVar::FontSize));
        ui.BeginGrid("show_stash", 4, { 72.f, 72.f }, 8.f);
        for (std::size_t i = 0; i < mShowSlots.size(); ++i)
        {
            const auto id   = std::string("show_") + std::to_string(i);
            auto&      slot = mShowSlots[i];
            const fra::TextureHandle icon =
                slot ? slot->icon : fra::TextureHandle {};
            const int stack = slot ? slot->stack : 0;
            if (ui.ItemSlot(id, icon, stack, false, { 72.f, 72.f }) && slot)
                mDnDLog = "Clicked " + slot->name + ".";
            if (slot && ui.IsItemHovered())
            {
                if (ui.BeginDragDropSource("show_item",
                                           static_cast<std::uint64_t>(i),
                                           slot->icon))
                {
                }
                if (ui.BeginTooltip("show_tip"))
                {
                    ui.Label(slot->name);
                    ui.EndTooltip();
                }
            }
            if (slot)
            {
                const auto ctxId = std::string("show_ctx_") + std::to_string(i);
                if (ui.BeginPopupContextItem(ctxId))
                {
                    if (ui.Button("Inspect", { 140.f, 28.f }))
                        mDnDLog = "Inspected " + slot->name + ".";
                    if (slot && ui.Button("Toss", { 140.f, 28.f }))
                    {
                        mDnDLog = "Tossed " + slot->name + ".";
                        slot.reset();
                    }
                    ui.EndPopup();
                }
            }
            std::uint64_t from = 0;
            if (ui.AcceptDragDropPayload("show_item", &from))
            {
                const auto src = static_cast<std::size_t>(from);
                if (src < mShowSlots.size() && src != i)
                {
                    std::swap(mShowSlots[src], mShowSlots[i]);
                    mDnDLog = "Moved stash slot " + std::to_string(src) + ".";
                }
            }
        }
        ui.EndGrid();
        ui.Label(mDnDLog.empty() ? "Drag between slots; right-click for "
                                   "popup; hover for tooltip."
                                 : mDnDLog);
        ui.Separator();

        ui.Label("Ability slots (click casts)",
                 ui.Style().Var(fra::UiVar::FontSize));
        ui.BeginGrid("show_abilities", 6, { 64.f, 64.f }, 8.f);
        for (std::size_t i = 0; i < mAbilities.size(); ++i)
        {
            auto&       a   = mAbilities[i];
            const float rem = a.duration > 0.f ? a.remaining / a.duration : 0.f;
            const auto  id  = std::string("show_ab_") + std::to_string(i);
            if (ui.AbilitySlot(id, a.icon, a.hotkey, rem, { 64.f, 64.f }))
                tryActivateAbility(i);
        }
        ui.EndGrid();
        ui.Separator();

        ui.Label("IconBadge", ui.Style().Var(fra::UiVar::FontSize));
        ui.Image(mIconPotion, glm::vec2 { 64.f, 64.f });
        ui.IconBadge("99", ui.LastItemRect());
        ui.Label("Badge is drawn over LastItemRect().",
                 ui.Style().Var(fra::UiVar::FontSizeSmall));
        ui.Separator();

        ui.Label("Quest list (BeginList + SetScrollHereY)",
                 ui.Style().Var(fra::UiVar::FontSize));
        float qRow[] = { 700.f, 220.f };
        ui.BeginColumns("quest_row", 2, qRow);
        ui.BeginList("quest_list", { 660.f, 220.f },
                     static_cast<int>(mQuestLog.size()), 24.f);
        if (mQuestJump)
        {
            const float ratio =
                mQuestLog.empty() ? 0.f
                                  : float(mQuestSel) / float(mQuestLog.size());
            ui.SetScrollHereY(ratio);
            mQuestJump = false;
        }
        for (std::size_t i = 0; i < mQuestLog.size(); ++i)
        {
            if (ui.ListItem(static_cast<int>(i), mQuestLog[i],
                            int(i) == mQuestSel))
                mQuestSel = int(i);
        }
        ui.EndList();
        ui.NextColumn();
        if (ui.Button("Jump to #30", { 180.f, 32.f }))
        {
            mQuestSel  = 29;
            mQuestJump = true;
        }
        ui.Label("Selected: #" + std::to_string(mQuestSel + 1));
        ui.EndColumns();
        ui.Separator();

        ui.Label("Live paper-doll (ModelPreview)",
                 ui.Style().Var(fra::UiVar::FontSize));
        if (mDollPreview)
        {
            ui.ModelPreview("show_doll", mDollPreview->Texture(),
                            { 256.f, 256.f }, mDollPreview.get());
            ui.Label("Same preview as F1 — LMB drag orbits.",
                     ui.Style().Var(fra::UiVar::FontSizeSmall));
        }
        else
            ui.Label("(preview unavailable)");
        ui.EndScrollView();
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

    // --- Showcase state: basics ---
    float mXp01       = 0.35f;
    int   mDifficulty = 1;
    // --- Showcase state: inputs ---
    bool        mInputsLocked = false;
    bool        mCheckMusic   = true;
    bool        mCheckShadows = true;
    bool        mToggleVsync  = true;
    bool        mToggleRotate = true;
    float       mVolume01     = 0.7f;
    int         mBrightness   = 80;
    int         mPartySize    = 4;
    int         mRelic        = 0;
    int         mClassIdx     = 0;
    glm::vec3   mTintRgb { 0.6f, 0.7f, 1.f };
    std::string mSearchBuf;
    std::string mHeroName = "Asha";
    int         mInputSel = 0;
    // --- Showcase state: layout ---
    std::string mGestureMsg   = "No gesture yet.";
    std::string mClipText     = "copy me";
    bool        mLayoutLocked = false;
    // --- Showcase state: windows/nav ---
    bool                     mSecondWindowOpen = false;
    bool                     mInspectorOpen    = true;
    int                      mSwitcherPage     = 0;
    int                      mWizardStep       = 0;
    int                      mShopPage         = 0;
    std::string              mFilePath;
    std::vector<std::string> mShopNames;
    // --- Showcase state: game tab ---
    std::array<std::optional<InvItem>, 8> mShowSlots {};
    std::string                           mDnDLog;
    std::vector<std::string>              mQuestLog;
    int                                   mQuestSel  = 0;
    bool                                  mQuestJump = false;
};

int main(int, const char**)
{
    return fra::RunApp<MainApp>(
        [](fra::FreyaOptionsBuilder& freyaOptions) {
            freyaOptions
                .SetTitle("GameUiShowcase — Freya UI [1-6 skills | "
                          "F1/F2/F3/F4 | RMB+WASD]")
                .SetWidth(1920)
                .SetHeight(1080)
                .SetVSync(false)
                .SetSampleCount(4)
                .WithReverseZ()
                .SetFullscreen(false)
                .SetEnableSsao(true)
                .SetEnableTaa(true)
                .SetEnableBloom(true);
        },
        [](skr::LoggingExtension& l) {
            FreyaExamples::ConfigureLogging(l, "GameUiShowcase.log");
        });
}
