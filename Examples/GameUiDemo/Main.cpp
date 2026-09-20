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

    struct InvItem
    {
        std::string       name;
        fra::TextureHandle icon {};
        int               stack = 1;
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

    void StartUp() override
    {
        mMainCam.window = mWindow;
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
                if (event.key == fra::KeyCode::Num1 ||
                    event.key == fra::KeyCode::Kp1)
                {
                    mScreen = mScreen == UiScreen::Inventory ? UiScreen::None
                                                            : UiScreen::Inventory;
                    return;
                }
                if (event.key == fra::KeyCode::Num2 ||
                    event.key == fra::KeyCode::Kp2)
                {
                    mScreen = mScreen == UiScreen::Dialogue ? UiScreen::None
                                                           : UiScreen::Dialogue;
                    return;
                }
                if (event.key == fra::KeyCode::Num3 ||
                    event.key == fra::KeyCode::Kp3)
                {
                    const bool open = mScreen != UiScreen::Chat;
                    mScreen =
                        open ? UiScreen::Chat : UiScreen::None;
                    if (open)
                        mChatFocusPending = true;
                    return;
                }
            });

        mFont = fra::FontAtlas::Create(
            *mTexturePool, "./Resources/Fonts/NotoSans-Regular.ttf");
        if (!mFont.Valid())
            std::cerr << "Failed to load NotoSans-Regular.ttf\n";

        mIconSword = MakeSolidTexture(*mTexturePool, 180, 80, 40);
        mIconPotion = MakeSolidTexture(*mTexturePool, 40, 160, 90);
        mIconGem    = MakeSolidTexture(*mTexturePool, 60, 100, 200);
        mPortrait   = MakeSolidTexture(*mTexturePool, 90, 70, 55, 128);

        mSlots.fill(std::nullopt);
        mSlots[0] = InvItem { "Iron Sword", mIconSword, 1 };
        mSlots[1] = InvItem { "Health Potion", mIconPotion, 3 };
        mSlots[2] = InvItem { "Sapphire", mIconGem, 12 };
        mSlots[5] = InvItem { "Health Potion", mIconPotion, 1 };
        mSlots[8] = InvItem { "Iron Sword", mIconSword, 1 };

        mChatLog = {
            "[System] Welcome to GameUiDemo.",
            "[Hint] Press 1 inventorio, 2 dialogo, 3 chat.",
            "[Hint] RMB+WASD to look/move; Esc closes UI.",
        };
        mChatInput.clear();

        mGroundMesh = FreyaExamples::CreateGroundPlane(
            *mMeshPool, 40.0f, glm::vec3(0.35f, 0.36f, 0.38f));
        mGroundMaterial = mMaterialPool->Create({});

        mLightService->AddLight(fra::MakeDirectionalLight(
            glm::vec3(-0.3f, -1.0f, -0.2f), glm::vec3(1.0f, 0.96f, 0.9f),
            1.8f));

        rebuildScene();

        std::cout
            << "GameUiDemo — native Freya UI (not ImGui)\n"
            << "  1 Inventory | 2 Dialogue | 3 Chat | Esc close\n"
            << "  RMB look | WASD move | ImGui debug panel still available\n";
    }

    void Update() override
    {
        mOverlay.MarkUpdateStart();
        mOverlay.BeginFrame();

        const float dt = mWindow->GetDeltaTime();
        mTime += dt;
        mMainCam.Update(dt);
        mHpPulse = 0.55f + 0.45f * std::sin(mTime * 0.8f);

        mRenderer->BeginFrame();
        mMainCam.Apply(*mRenderer);
        mScene.Upload(*mRenderer);

        drawGameUi(dt);

        const float cpuFrameMs  = mWindow->GetDeltaTime() * 1000.f;
        const float cpuUpdateMs = mOverlay.ElapsedUpdateMs();
        mOverlay.Draw(*mRenderer, *mFreyaOptions, cpuFrameMs, cpuUpdateMs,
                      mLightService.get());
        mOverlay.EndFrame(*mRenderer);
    }

  private:
    void rebuildScene()
    {
        mScene.Clear();
        fra::Scene::Instance ground {};
        ground.mesh     = mGroundMesh;
        ground.material = mGroundMaterial;
        ground.mobility = fra::Mobility::Static;
        ground.transform = fra::SceneTransform::FromMatrix(
            glm::translate(glm::mat4(1.f), glm::vec3(0.f, -1.f, 0.f)));
        mScene.Add(ground);
    }

    void drawGameUi(float dt)
    {
        auto& ui = mRenderer->GetUiContext();
        ui.Style().font = mFont.Valid() ? &mFont : nullptr;

        const auto w = mFreyaOptions->width;
        const auto h = mFreyaOptions->height;
        ui.Begin(dt, { w, h });

        drawHud(ui);

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

    void drawHud(fra::UiContext& ui)
    {
        ui.BeginAnchor(fra::UiAnchor::TopLeft, { 24.f, 24.f });
        ui.Label("HP", ui.Style().Var(fra::UiVar::FontSizeSmall));
        ui.ProgressBar(mHpPulse, { 220.f, 18.f });
        ui.EndAnchor();

        ui.BeginAnchor(fra::UiAnchor::TopRight, { -360.f, 24.f });
        ui.Label("1 Inv  |  2 Dialog  |  3 Chat",
                 ui.Style().Var(fra::UiVar::FontSizeSmall));
        ui.EndAnchor();
    }

    void drawInventory(fra::UiContext& ui)
    {
        if (!ui.BeginModal("inventory", { 520.f, 420.f }))
            return;

        ui.Label("Inventory", ui.Style().Var(fra::UiVar::FontSizeTitle));
        ui.Separator();

        constexpr int   kCols = 5;
        constexpr float kCell = 72.f;
        constexpr float kGap  = 8.f;
        ui.BeginGrid("inv_grid", kCols, { kCell, kCell }, kGap);

        for (std::size_t i = 0; i < mSlots.size(); ++i)
        {
            const auto id =
                std::string("slot_") + std::to_string(i);
            auto& slot = mSlots[i];
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

            // Keep calling while open so the menu survives hover loss.
            if (slot)
            {
                const auto ctxId =
                    std::string("ctx_") + std::to_string(i);
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
        }

        ui.EndGrid();

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
                std::cout << "Dialogue choice: "
                          << (help ? "help" : "decline") << '\n';
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

    UiScreen                             mScreen = UiScreen::None;
    std::array<std::optional<InvItem>, 20> mSlots {};
    float                                mHpPulse = 1.f;
    float                                mTime    = 0.f;

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
            freyaOptions.SetTitle("GameUiDemo — Freya UI [1/2/3 | RMB+WASD]")
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
