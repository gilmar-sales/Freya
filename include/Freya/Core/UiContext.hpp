#pragma once

#include "Freya/Core/UiDraw.hpp"
#include "Freya/Core/UiStyle.hpp"
#include "Freya/Core/UiTypes.hpp"
#include "Freya/Events/EventManager.hpp"
#include "Freya/Events/KeyCode.hpp"
#include "Freya/Events/Keyboard.hpp"
#include "Freya/Events/Mouse.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    class UiModelPreview;

    /**
     * @brief Immediate-mode game UI (main-thread). Draws into an owned or
     * shared UiDraw queue; interaction state is retained by UiId.
     */
    class UiContext
    {
      public:
        explicit UiContext(UiDraw* draw = nullptr);

        void                  SetDraw(UiDraw* draw) { mDraw = draw; }
        [[nodiscard]] UiDraw* GetDraw() const { return mDraw; }

        UiStyle&       Style() { return mStyle; }
        const UiStyle& Style() const { return mStyle; }
        void           SetStyle(const UiStyle& style) { mStyle = style; }

        void PushStyleColor(UiCol col, const glm::vec4& color);
        void PopStyleColor(int count = 1);
        void PushStyleVar(UiVar var, float value);
        void PopStyleVar(int count = 1);

        /**
         * @brief Call once per frame after PumpEvents, before widgets.
         * @param fbExtent Swapchain/framebuffer size in pixels.
         */
        void Begin(float dt, glm::uvec2 fbExtent);
        void End();

        void SetReferenceSize(glm::vec2 logical) { mRefSize = logical; }
        [[nodiscard]] glm::vec2 ReferenceSize() const { return mRefSize; }

        /**
         * @brief UI scale (logical px -> framebuffer px) for a framebuffer
         * size: the smaller axis ratio against the reference size, so the UI
         * grows with resolution (4K shows the same layout as 1080p).
         */
        [[nodiscard]] float ScaleForExtent(glm::uvec2 fbExtent) const
        {
            const float sx = fbExtent.x > 0
                                 ? static_cast<float>(fbExtent.x) / mRefSize.x
                                 : 1.f;
            const float sy = fbExtent.y > 0
                                 ? static_cast<float>(fbExtent.y) / mRefSize.y
                                 : 1.f;
            return sx < sy ? sx : sy;
        }

        /// True once Begin() has run at least once (the context drives
        /// its own scale); false for apps that never use game UI.
        [[nodiscard]] bool HasBegun() const { return mHasBegun; }
        void SetSafeArea(UiRect insets) { mSafeArea = insets; }

        [[nodiscard]] float     Scale() const { return mScale; }
        [[nodiscard]] glm::vec2 LogicalSize() const { return mLogicalSize; }

        [[nodiscard]] bool WantCaptureMouse() const { return mWantMouse; }
        [[nodiscard]] bool WantCaptureKeyboard() const { return mWantKeyboard; }
        [[nodiscard]] bool WantCaptureGamepad() const { return mWantGamepad; }
        [[nodiscard]] bool WantTextInput() const { return mWantTextInput; }
        [[nodiscard]] UiMouseCursor MouseCursor() const { return mMouseCursor; }

        void BindEvents(EventManager& events);
        void UnbindEvents(EventManager& events);

        /**
         * @brief Set the UI pointer in active framebuffer pixel space.
         *
         * Call after PumpEvents / before Begin when the host remaps window
         * coords into an offscreen RT (e.g. editor gameplay viewport).
         * Overrides the last MouseMoveEvent fed via BindEvents.
         */
        void SetPointerFramebuffer(float x, float y);

        // --- Layout ---
        void BeginAnchor(UiAnchor anchor, glm::vec2 offset = { 0.f, 0.f });
        void EndAnchor();
        void PushParent(const UiRect& rect);
        void PopParent();
        [[nodiscard]] UiRect CurrentParent() const;

        void Background(TextureHandle    texture,
                        UiImageFit       fit  = UiImageFit::Cover,
                        const glm::vec4& tint = { 1.f, 1.f, 1.f, 1.f });
        void Image(TextureHandle texture, glm::vec2 size,
                   const UiImageOpts& opts = {});
        void Image(TextureHandle texture, const UiRect& rect,
                   const UiImageOpts& opts = {});

        /**
         * @brief Draw a live/static model preview texture with optional orbit.
         *
         * When @p orbitTarget is non-null and Orbit().enabled, drag on the
         * widget drives FeedMouse*; SetFrameDelta(mDt) is applied each call.
         */
        void ModelPreview(std::string_view id, TextureHandle texture,
                          glm::vec2       size,
                          UiModelPreview* orbitTarget = nullptr);

        bool BeginPanel(std::string_view id, glm::vec2 size,
                        const UiPanelOpts& opts = {});
        void EndPanel();

        bool BeginModal(std::string_view id, glm::vec2 size = { 480.f, 360.f },
                        const UiPanelOpts& opts = {});
        void EndModal();

        bool BeginWindow(std::string_view id, std::string_view title,
                         glm::vec2 size, const UiWindowOpts& opts = {});
        void EndWindow();
        [[nodiscard]] UiRect WindowRect(std::string_view id) const;
        [[nodiscard]] bool   IsWindowOpen(std::string_view id) const;
        void                 SetWindowOpen(std::string_view id, bool open);

        bool BeginSwitcher(std::string_view id, glm::vec2 size);
        void EndSwitcher();

        bool BeginWizard(std::string_view                  id,
                         std::span<const std::string_view> steps, int current,
                         glm::vec2 size);
        void EndWizard();
        /**
         * @brief Back / Next-or-Finish row. @return -1 back, +1 next/finish,
         * 0 none.
         */
        int WizardNav(std::string_view id, int current, int stepCount,
                      glm::vec2 size = { 0.f, 36.f });

        bool BeginDrawer(std::string_view id, UiAnchor side, float width);
        void EndDrawer();
        [[nodiscard]] bool IsDrawerOpen(std::string_view id) const;
        void               OpenDrawer(std::string_view id, bool open = true);

        /**
         * @brief Pager row ("< 1/5 >"). @return true when *page changed.
         * Layout: {220,32}, prev {40,32} left, next {40,32} right.
         */
        bool        Paginate(std::string_view id, int itemCount, int pageSize,
                             int* page);
        static void PageRange(int itemCount, int pageSize, int page, int* first,
                              int* count);

        void OpenFileDialog(std::string_view id, std::string directory);
        bool FileDialog(std::string_view id, std::string_view title,
                        const UiFileDialogOpts& opts, std::string* outPath,
                        glm::vec2 size = { 560.f, 420.f });

        void Label(std::string_view text, float fontSize = 0.f,
                   const glm::vec4* color = nullptr);
        void TextWrapped(std::string_view text, float maxWidth,
                         float fontSize = 0.f);
        // --- Group 2: display widgets ---
        void Heading(std::string_view text);
        void Bullet(std::string_view text, float fontSize = 0.f);
        void LabelColored(std::string_view text, const glm::vec4& color,
                          float fontSize = 0.f);
        bool CollapsingHeader(std::string_view id, std::string_view label,
                              bool defaultOpen = true);
        void Spinner(std::string_view id, glm::vec2 size = { 24.f, 24.f });
        void ShowToast(std::string text, float duration = 2.5f);

        void ProgressBar(float fill01, glm::vec2 size);
        void Separator();

        bool Button(std::string_view label, glm::vec2 size = { 160.f, 40.f });
        bool Checkbox(std::string_view label, bool* value);
        bool SliderFloat(std::string_view label, float* value, float vMin,
                         float vMax, glm::vec2 size = { 200.f, 24.f });
        // --- Group 1: input widgets ---
        bool RadioButton(std::string_view label, bool active,
                         glm::vec2 size = { 200.f, 24.f });
        bool ToggleSwitch(std::string_view label, bool* value,
                          glm::vec2 size = { 52.f, 28.f });
        bool SliderInt(std::string_view label, int* value, int vMin, int vMax,
                       glm::vec2 size = { 200.f, 24.f });
        bool SpinBox(std::string_view id, int* value, int vMin, int vMax,
                     int step = 1, glm::vec2 size = { 200.f, 32.f });
        bool ComboBox(std::string_view                  id,
                      std::span<const std::string_view> items, int* selected,
                      glm::vec2 size = { 220.f, 32.f });
        bool ColorEdit(std::string_view id, glm::vec3* rgb);
        bool SearchBox(std::string_view id, std::string& buffer,
                       std::size_t maxLen, glm::vec2 size = { 220.f, 32.f });
        bool Selectable(std::string_view label, bool selected,
                        glm::vec2 size = { 0.f, 32.f });

        bool BeginScrollView(std::string_view id, glm::vec2 size,
                             float contentHeight);
        void EndScrollView();
        void SetScrollHereY(float ratio = 1.f);

        bool BeginList(std::string_view id, glm::vec2 size, int itemCount,
                       float itemHeight, int* scrollIndex = nullptr);
        bool ListItem(int index, std::string_view label, bool selected);
        void EndList();

        bool BeginGrid(std::string_view id, int cols, glm::vec2 cellSize,
                       float gap = 4.f);
        void EndGrid();

        bool BeginTabBar(std::string_view id);
        bool Tab(std::string_view label);
        void EndTabBar();

        bool ItemSlot(std::string_view id, TextureHandle icon, int stackCount,
                      bool selected, glm::vec2 size = { 64.f, 64.f });
        void IconBadge(std::string_view text, const UiRect& slot);

        /**
         * @brief Ability / skill slot with hotkey badge and radial cooldown.
         * @param cooldownRemaining01 1 = just triggered, 0 = ready.
         * @return true when activated (click) while ready.
         */
        bool AbilitySlot(std::string_view id, TextureHandle icon,
                         std::string_view hotkey, float cooldownRemaining01,
                         glm::vec2 size = { 64.f, 64.f });

        bool BeginTooltip(std::string_view id, float delay = 0.35f);
        void EndTooltip();

        bool BeginPopupContextItem(std::string_view id);
        void EndPopup();

        bool BeginDragDropSource(std::string_view type, std::uint64_t payload,
                                 TextureHandle preview = {});
        bool AcceptDragDropPayload(std::string_view type,
                                   std::uint64_t*   outPayload);
        [[nodiscard]] bool IsDragDropActive() const { return mDrag.active; }

        bool TextInput(std::string_view id, std::string& buffer,
                       std::size_t maxLen, glm::vec2 size = { 320.f, 32.f });

        /** Focus a TextInput on the next Begin/widget pass (IME on). */
        void FocusTextInput(std::string_view id, std::string_view text = {});

        bool BeginColumns(std::string_view id, int count, float widths[]);
        void NextColumn();
        void EndColumns();

        // --- Group 3: layout containers ---
        bool BeginRow(std::string_view id, std::span<const float> weights,
                      float gap = 8.f, float height = 0.f);
        void NextCell();
        void EndRow();
        bool BeginMargin(std::string_view id, float pad);
        void EndMargin();
        bool BeginCenter(std::string_view id, glm::vec2 size);
        void EndCenter();
        bool BeginVStack(std::string_view id, float gap = 8.f);
        void EndVStack();

        // --- Group 6: interaction state ---
        [[nodiscard]] bool IsItemDoubleClicked() const
        {
            return mLastDoubleClicked;
        }
        [[nodiscard]] bool IsItemLongPressed(float duration = 0.6f) const;
        void               BeginDisabled(bool disabled = true);
        void               EndDisabled();
        [[nodiscard]] bool IsDisabled() const { return mDisabledDepth > 0; }
        void SetClipboard(std::string text) { mClipboard = std::move(text); }
        [[nodiscard]] const std::string& Clipboard() const
        {
            return mClipboard;
        }

        [[nodiscard]] bool   IsItemHovered() const { return mLastHovered; }
        [[nodiscard]] bool   IsItemActive() const { return mLastActive; }
        [[nodiscard]] bool   IsItemClicked() const { return mLastClicked; }
        [[nodiscard]] UiRect LastItemRect() const { return mLastRect; }

      private:
        struct StyleColorMod
        {
            UiCol     col;
            glm::vec4 backup;
        };
        struct StyleVarMod
        {
            UiVar var;
            float backup;
        };
        struct ParentFrame
        {
            UiRect rect;
            float  cursorX = 0.f;
            float  cursorY = 0.f;
            float  lineH   = 0.f;
        };
        struct WidgetState
        {
            bool      open       = false;
            float     scrollY    = 0.f;
            float     hoverTime  = 0.f;
            float     hoverT     = 0.f;
            int       activeTab  = 0;
            int       focusIndex = 0;
            bool      dragging   = false;
            glm::vec2 popupPos { 0.f, 0.f };
            UiRect    popupRect {};
            // Group 4: floating windows / dialogs
            glm::vec2   winPos { 0.f, 0.f };
            glm::vec2   winSize { 0.f, 0.f };
            bool        winInit     = false;
            bool        collapsed   = false;
            bool        winDragging = false;
            bool        winResizing = false;
            glm::vec2   grabOffset { 0.f, 0.f };
            std::string dialogDir;
            std::string dialogSel;
        };
        struct Focusable
        {
            UiId   id;
            UiRect rect;
            int    layer = 0;
        };
        struct DragPayload
        {
            std::string   type;
            std::uint64_t value  = 0;
            bool          active = false;
            TextureHandle preview {};
        };

        [[nodiscard]] UiId HashId(std::string_view id) const;
        [[nodiscard]] UiId HashId(std::string_view id, int index) const;
        WidgetState&       State(UiId id);
        // Identity/display split for composite widgets (SpinBox, ColorEdit):
        // public Slider* keeps label==id, internals pass explicit ids with
        // their own display text (or empty to skip the label row).
        bool SliderFloatImpl(UiId id, std::string_view label, float* value,
                             float vMin, float vMax, glm::vec2 size);
        bool SliderIntImpl(UiId id, std::string_view label, int* value,
                           int vMin, int vMax, glm::vec2 size);
        void AdvanceCursor(glm::vec2 size);
        [[nodiscard]] UiRect Place(glm::vec2 size);
        void                 DrawFocusRing(const UiRect& r);
        bool                 Hit(const UiRect& r) const;
        void                 RegisterFocusable(UiId id, const UiRect& r);
        void                 FeedMouseMove(float x, float y);
        void                 FeedMouseButton(MouseButton button, bool down);
        void                 FeedKey(KeyCode key, bool down);
        void                 FeedText(std::string_view text);
        void                 FeedScroll(float dy);
        void                 ProcessNav();

        UiDraw* mDraw  = nullptr;
        UiStyle mStyle = UiStyle::Default();

        std::vector<StyleColorMod> mColorStack;
        std::vector<StyleVarMod>   mVarStack;
        std::vector<ParentFrame>   mParents;

        glm::vec2  mRefSize { 1920.f, 1080.f };
        glm::vec2  mLogicalSize { 1920.f, 1080.f };
        glm::uvec2 mFbExtent { 1, 1 };
        float      mScale = 1.f;
        bool       mHasBegun = false;
        float      mDt    = 0.f;
        UiRect     mSafeArea {};

        float mMouseX        = 0.f;
        float mMouseY        = 0.f;
        float mMouseLogicalX = 0.f;
        float mMouseLogicalY = 0.f;
        bool  mMouseDown[8] {};
        bool  mMouseClicked[8] {};
        bool  mMouseReleased[8] {};
        float mWheel = 0.f;

        bool          mWantMouse     = false;
        bool          mWantKeyboard  = false;
        bool          mWantGamepad   = false;
        bool          mWantTextInput = false;
        UiMouseCursor mMouseCursor   = UiMouseCursor::Arrow;

        bool   mLastHovered = false;
        bool   mLastActive  = false;
        bool   mLastClicked = false;
        UiRect mLastRect {};
        UiId   mLastId             = 0;
        UiId   mHotId              = 0;
        UiId   mActiveId           = 0;
        UiId   mFocusId            = 0;
        UiId   mTextInputId        = 0;
        bool   mTextSubmit         = false;
        UiId   mPendingTextFocusId = 0;
        bool   mPendingTextFocus   = false;

        int  mModalLayer    = 0;
        bool mCloseTopModal = false;

        std::unordered_map<UiId, WidgetState> mStates;
        std::vector<Focusable>                mFocusables;
        DragPayload                           mDrag;
        std::string                           mTextEditBuffer;
        std::vector<EventSubscription>        mSubs;

        // Grid / list / columns / tabs transient
        struct GridCtx
        {
            int       cols = 1;
            glm::vec2 cell { 64.f };
            float     gap   = 4.f;
            int       index = 0;
            UiRect    area {};
        };
        std::vector<GridCtx> mGrids;

        struct ListCtx
        {
            int    firstVisible = 0;
            int    visibleCount = 0;
            float  itemHeight   = 28.f;
            int    itemCount    = 0;
            UiRect area {};
        };
        std::vector<ListCtx> mLists;

        struct TabCtx
        {
            UiId id;
            int  index = 0;
            int  drawn = 0;
        };
        std::vector<TabCtx> mTabs;

        struct ColCtx
        {
            int    count = 1;
            int    index = 0;
            float  widths[8] {};
            UiRect area {};
            float  startY    = 0.f;
            float  maxBottom = 0.f; ///< flow resumes below tallest column
        };
        std::vector<ColCtx> mColumns;

        struct ScrollCtx
        {
            UiId   id;
            UiRect view {};
            float  contentH = 0.f;
        };
        std::vector<ScrollCtx> mScrolls;

        bool mInTooltip     = false;
        bool mPopupOpen     = false;
        UiId mPopupId       = 0;
        int  mOverlayDepth  = 0;
        bool mTextInputSeen = false;

        // Group 6/7 runtime
        float       mTime              = 0.f;
        bool        mLastDoubleClicked = false;
        UiId        mLastClickId       = 0;
        float       mLastClickTime     = -10.f;
        UiId        mPressId           = 0;
        float       mPressTime         = -10.f;
        bool        mCtrlHeld          = false;
        int         mDisabledDepth     = 0;
        std::string mClipboard;
        std::size_t mTextCursor      = 0;
        float       mVStackGapBackup = 8.f;
        bool        mInVStack        = false;

        struct Toast
        {
            std::string text;
            float       remaining = 0.f;
            float       duration  = 1.f;
        };
        std::vector<Toast> mToasts;

        [[nodiscard]] glm::vec4 AnimMix(const glm::vec4& a, const glm::vec4& b,
                                        float t) const
        {
            return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t,
                     a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t };
        }
        void                             TrackClick(UiId id, const UiRect& r);
        void                             DrawToasts();
        [[nodiscard]] glm::vec4          ApplyDisabled(glm::vec4 c) const;
        [[nodiscard]] WidgetState*       FindState(UiId id);
        [[nodiscard]] const WidgetState* FindState(UiId id) const;

        struct WindowFrame
        {
            UiId id;
            bool content = false;
        };
        std::vector<WindowFrame> mWindows;
        UiId                     mWinActiveId = 0;
    };

} // namespace FREYA_NAMESPACE
