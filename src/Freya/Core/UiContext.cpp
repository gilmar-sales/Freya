#include "Freya/Core/UiContext.hpp"

#include "Freya/Core/UiModelPreview.hpp"
#include "Freya/Events/Gamepad.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace FREYA_NAMESPACE
{
    namespace
    {
        constexpr std::uint32_t Fnv1a(std::string_view s)
        {
            std::uint32_t h = 2166136261u;
            for (unsigned char c : s)
            {
                h ^= c;
                h *= 16777619u;
            }
            return h;
        }
    } // namespace

    UiContext::UiContext(UiDraw* draw) : mDraw(draw)
    {
    }

    void UiContext::PushStyleColor(UiCol col, const glm::vec4& color)
    {
        mColorStack.push_back({ col, mStyle.Color(col) });
        mStyle.Color(col) = color;
    }

    void UiContext::PopStyleColor(int count)
    {
        while (count-- > 0 && !mColorStack.empty())
        {
            auto mod = mColorStack.back();
            mColorStack.pop_back();
            mStyle.Color(mod.col) = mod.backup;
        }
    }

    void UiContext::PushStyleVar(UiVar var, float value)
    {
        mVarStack.push_back({ var, mStyle.Var(var) });
        mStyle.Var(var) = value;
    }

    void UiContext::PopStyleVar(int count)
    {
        while (count-- > 0 && !mVarStack.empty())
        {
            auto mod = mVarStack.back();
            mVarStack.pop_back();
            mStyle.Var(mod.var) = mod.backup;
        }
    }

    void UiContext::Begin(float dt, glm::uvec2 fbExtent)
    {
        mDt       = dt;
        mFbExtent = fbExtent;
        const float sx =
            fbExtent.x > 0 ? static_cast<float>(fbExtent.x) / mRefSize.x : 1.f;
        const float sy =
            fbExtent.y > 0 ? static_cast<float>(fbExtent.y) / mRefSize.y : 1.f;
        mScale         = std::min(sx, sy);
        mLogicalSize   = glm::vec2 { static_cast<float>(fbExtent.x) / mScale,
                                   static_cast<float>(fbExtent.y) / mScale };
        mMouseLogicalX = mMouseX / mScale;
        mMouseLogicalY = mMouseY / mScale;

        mWantMouse = mWantKeyboard = mWantGamepad = false;
        mWantTextInput                            = false;
        mTextInputSeen                            = false;
        mMouseCursor                              = UiMouseCursor::Arrow;
        mFocusables.clear();
        mParents.clear();
        mGrids.clear();
        mLists.clear();
        mTabs.clear();
        mColumns.clear();
        mScrolls.clear();
        mLastHovered = mLastActive = mLastClicked = false;
        mLastRect                                 = {};
        mLastId                                   = 0;
        mCloseTopModal                            = false;
        mInTooltip                                = false;
        mOverlayDepth                             = 0;
        // mTextSubmit is set by PumpEvents (Return) before Begin; consume
        // in TextInput and clear in End.
        // Mouse/wheel edges are filled by PumpEvents before Begin; consume
        // them during widgets and clear in End().

        UiRect root { mSafeArea.x, mSafeArea.y,
                      mLogicalSize.x - mSafeArea.x - mSafeArea.w,
                      mLogicalSize.y - mSafeArea.y - mSafeArea.h };
        mParents.push_back({ root, root.x, root.y, 0.f });
    }

    void UiContext::End()
    {
        if (mDrag.active)
        {
            mMouseCursor = UiMouseCursor::Move;
            mWantMouse   = true;
            if (mDraw && mDrag.preview.IsValid())
            {
                mDraw->BeginOverlay();
                constexpr float kS = 56.f;
                UiImageOpts     opts {};
                opts.tint     = { 1.f, 1.f, 1.f, 0.88f };
                opts.rounding = 6.f;
                mDraw->Rect(
                    { mMouseLogicalX + 10.f, mMouseLogicalY + 10.f, kS, kS },
                    { 0.08f, 0.09f, 0.10f, 0.75f }, 6.f, 1.f,
                    mStyle.Color(UiCol::Border));
                mDraw->Image({ mMouseLogicalX + 14.f, mMouseLogicalY + 14.f,
                               kS - 8.f, kS - 8.f },
                             mDrag.preview, opts);
                mDraw->EndOverlay();
            }
            if (!mMouseDown[static_cast<int>(MouseButton::Left)])
                mDrag.active = false;
        }

        ProcessNav();
        while (!mColorStack.empty())
            PopStyleColor();
        while (!mVarStack.empty())
            PopStyleVar();
        mParents.clear();
        if (mModalLayer > 0)
        {
            mWantMouse = mWantKeyboard = mWantGamepad = true;
        }
        if (!mTextInputSeen)
        {
            mWantTextInput = false;
            mTextInputId   = 0;
        }
        for (auto& c : mMouseClicked)
            c = false;
        for (auto& c : mMouseReleased)
            c = false;
        mWheel      = 0.f;
        mTextSubmit = false;
    }

    void UiContext::BindEvents(EventManager& events)
    {
        UnbindEvents(events);
        mSubs.push_back(events.Subscribe<MouseMoveEvent>(
            [this](MouseMoveEvent& e) { FeedMouseMove(e.x, e.y); }));
        mSubs.push_back(events.Subscribe<MouseButtonPressedEvent>(
            [this](MouseButtonPressedEvent& e) {
                FeedMouseButton(e.button, true);
            }));
        mSubs.push_back(events.Subscribe<MouseButtonReleasedEvent>(
            [this](MouseButtonReleasedEvent& e) {
                FeedMouseButton(e.button, false);
            }));
        mSubs.push_back(events.Subscribe<MouseWheelEvent>(
            [this](MouseWheelEvent& e) { FeedScroll(e.y); }));
        mSubs.push_back(events.Subscribe<KeyPressedEvent>(
            [this](KeyPressedEvent& e) { FeedKey(e.key, true); }));
        mSubs.push_back(events.Subscribe<TextInputEvent>(
            [this](TextInputEvent& e) { FeedText(e.text); }));
        mSubs.push_back(events.Subscribe<GamepadButtonPressedEvent>(
            [this](GamepadButtonPressedEvent& e) {
                mWantGamepad = mModalLayer > 0 || mFocusId != 0;
                if (e.button == GamepadButton::GamepadButtonEast &&
                    mModalLayer > 0)
                    mCloseTopModal = true;
                if (e.button == GamepadButton::GamepadButtonSouth &&
                    mFocusId != 0)
                    mActiveId = mFocusId;
                if (e.button == GamepadButton::GamepadButtonDpadUp ||
                    e.button == GamepadButton::GamepadButtonDpadDown ||
                    e.button == GamepadButton::GamepadButtonDpadLeft ||
                    e.button == GamepadButton::GamepadButtonDpadRight)
                {
                    // Defer to ProcessNav via focusables order
                    if (!mFocusables.empty())
                    {
                        int idx = 0;
                        for (std::size_t i = 0; i < mFocusables.size(); ++i)
                        {
                            if (mFocusables[i].id == mFocusId)
                            {
                                idx = static_cast<int>(i);
                                break;
                            }
                        }
                        if (e.button == GamepadButton::GamepadButtonDpadDown ||
                            e.button == GamepadButton::GamepadButtonDpadRight)
                            idx = (idx + 1) %
                                  static_cast<int>(mFocusables.size());
                        else
                            idx = (idx - 1 +
                                   static_cast<int>(mFocusables.size())) %
                                  static_cast<int>(mFocusables.size());
                        mFocusId =
                            mFocusables[static_cast<std::size_t>(idx)].id;
                    }
                }
            }));
    }

    void UiContext::UnbindEvents(EventManager& events)
    {
        // EventManager lacks typed unbind without knowing T; clear ids only.
        // Subscriptions are process-lifetime with renderer; no-op safe.
        (void) events;
        mSubs.clear();
    }

    UiId UiContext::HashId(std::string_view id) const
    {
        return Fnv1a(id);
    }

    UiId UiContext::HashId(std::string_view id, int index) const
    {
        return Fnv1a(id) ^ (static_cast<UiId>(index) * 0x9E3779B9u);
    }

    UiContext::WidgetState& UiContext::State(UiId id)
    {
        return mStates[id];
    }

    UiRect UiContext::CurrentParent() const
    {
        if (mParents.empty())
            return { 0, 0, mLogicalSize.x, mLogicalSize.y };
        return mParents.back().rect;
    }

    void UiContext::PushParent(const UiRect& rect)
    {
        mParents.push_back({ rect, rect.x, rect.y, 0.f });
    }

    void UiContext::PopParent()
    {
        if (mParents.size() > 1)
            mParents.pop_back();
    }

    void UiContext::AdvanceCursor(glm::vec2 size)
    {
        if (mParents.empty())
            return;
        auto& p = mParents.back();
        p.lineH = std::max(p.lineH, size.y);
        p.cursorY += size.y + mStyle.Var(UiVar::ItemSpacing);
        p.cursorX = p.rect.x;
        p.lineH   = 0.f;
    }

    UiRect UiContext::Place(glm::vec2 size)
    {
        // Tooltips/popups push their own parent; never consume grid/column
        // slots while laying out overlay content.
        if (mOverlayDepth == 0 && !mGrids.empty())
        {
            auto&     g   = mGrids.back();
            const int col = g.index % g.cols;
            const int row = g.index / g.cols;
            ++g.index;
            return { g.area.x + col * (g.cell.x + g.gap),
                     g.area.y + row * (g.cell.y + g.gap), g.cell.x, g.cell.y };
        }
        if (mOverlayDepth == 0 && !mColumns.empty())
        {
            auto& c = mColumns.back();
            float x = c.area.x;
            for (int i = 0; i < c.index && i < c.count; ++i)
                x += c.widths[i];
            float w = (c.index < c.count) ? c.widths[c.index] : size.x;
            if (size.x <= 0.f)
                size.x = w;
            auto&  p = mParents.back();
            UiRect r { x, p.cursorY, size.x, size.y };
            p.cursorY += size.y + mStyle.Var(UiVar::ItemSpacing);
            return r;
        }
        auto&  p = mParents.back();
        UiRect r { p.cursorX, p.cursorY, size.x, size.y };
        AdvanceCursor(size);
        return r;
    }

    bool UiContext::Hit(const UiRect& r) const
    {
        return r.Contains(mMouseLogicalX, mMouseLogicalY);
    }

    void UiContext::RegisterFocusable(UiId id, const UiRect& r)
    {
        mFocusables.push_back({ id, r, mModalLayer });
    }

    void UiContext::DrawFocusRing(const UiRect& r)
    {
        if (!mDraw || mFocusId == 0)
            return;
        mDraw->Rect(
            r.Expand(2.f), { 0.f, 0.f, 0.f, 0.f }, mStyle.Var(UiVar::Rounding),
            mStyle.Var(UiVar::FocusRingWidth), mStyle.Color(UiCol::FocusRing));
    }

    void UiContext::SetPointerFramebuffer(float x, float y)
    {
        mMouseX = x;
        mMouseY = y;

        if (mScale > 0.f)
        {
            mMouseLogicalX = x / mScale;
            mMouseLogicalY = y / mScale;
        }
    }

    void UiContext::FeedMouseMove(float x, float y)
    {
        SetPointerFramebuffer(x, y);
    }

    void UiContext::FeedMouseButton(MouseButton button, bool down)
    {
        const int i = static_cast<int>(button);
        if (i < 0 || i >= 8)
            return;
        if (down)
        {
            mMouseDown[i]    = true;
            mMouseClicked[i] = true;
        }
        else
        {
            mMouseDown[i]     = false;
            mMouseReleased[i] = true;
        }
    }

    void UiContext::FeedKey(KeyCode key, bool down)
    {
        if (!down)
            return;
        if (key == KeyCode::Escape && mModalLayer > 0)
            mCloseTopModal = true;
        if (key == KeyCode::Tab && !mFocusables.empty())
        {
            int idx = 0;
            for (std::size_t i = 0; i < mFocusables.size(); ++i)
            {
                if (mFocusables[i].id == mFocusId)
                {
                    idx = static_cast<int>(i);
                    break;
                }
            }
            idx           = (idx + 1) % static_cast<int>(mFocusables.size());
            mFocusId      = mFocusables[static_cast<std::size_t>(idx)].id;
            mWantKeyboard = true;
        }
        if (mTextInputId != 0 && key == KeyCode::Backspace &&
            !mTextEditBuffer.empty())
        {
            mTextEditBuffer.pop_back();
            while (!mTextEditBuffer.empty() &&
                   (static_cast<unsigned char>(mTextEditBuffer.back()) &
                    0xC0) == 0x80)
                mTextEditBuffer.pop_back();
        }
        if (mTextInputId != 0 && key == KeyCode::Return)
            mTextSubmit = true;
    }

    void UiContext::FeedText(std::string_view text)
    {
        if (mTextInputId == 0)
            return;
        mTextEditBuffer.append(text);
    }

    void UiContext::FeedScroll(float dy)
    {
        mWheel += dy;
    }

    void UiContext::ProcessNav()
    {
        if (mCloseTopModal && mModalLayer > 0)
        {
            // Signal handled by EndModal checking flag
        }
    }

    void UiContext::BeginAnchor(UiAnchor anchor, glm::vec2 offset)
    {
        const UiRect p = CurrentParent();
        glm::vec2    pos { p.x, p.y };
        switch (anchor)
        {
            case UiAnchor::TopLeft:
                pos = { p.x, p.y };
                break;
            case UiAnchor::Top:
                pos = { p.x + p.w * 0.5f, p.y };
                break;
            case UiAnchor::TopRight:
                pos = { p.x + p.w, p.y };
                break;
            case UiAnchor::Left:
                pos = { p.x, p.y + p.h * 0.5f };
                break;
            case UiAnchor::Center:
                pos = { p.x + p.w * 0.5f, p.y + p.h * 0.5f };
                break;
            case UiAnchor::Right:
                pos = { p.x + p.w, p.y + p.h * 0.5f };
                break;
            case UiAnchor::BottomLeft:
                pos = { p.x, p.y + p.h };
                break;
            case UiAnchor::Bottom:
                pos = { p.x + p.w * 0.5f, p.y + p.h };
                break;
            case UiAnchor::BottomRight:
                pos = { p.x + p.w, p.y + p.h };
                break;
        }
        pos += offset;
        // Use remaining space as child parent starting at pos
        PushParent({ pos.x, pos.y, p.x + p.w - pos.x, p.y + p.h - pos.y });
    }

    void UiContext::EndAnchor()
    {
        PopParent();
    }

    void UiContext::Background(TextureHandle texture, UiImageFit fit,
                               const glm::vec4& tint)
    {
        if (!mDraw)
            return;
        const UiRect r = CurrentParent();
        if (texture.IsValid())
        {
            UiImageOpts o {};
            o.fit          = fit;
            o.tint         = tint;
            o.sliceMargins = mStyle.sliceMargins;
            mDraw->Image(r, texture, o);
        }
        else
        {
            mDraw->Rect(r, tint * mStyle.Color(UiCol::WindowBg),
                        mStyle.Var(UiVar::Rounding));
        }
    }

    void UiContext::Image(TextureHandle texture, glm::vec2 size,
                          const UiImageOpts& opts)
    {
        Image(texture, Place(size), opts);
    }

    void UiContext::Image(TextureHandle texture, const UiRect& rect,
                          const UiImageOpts& opts)
    {
        mLastRect = rect;
        if (!mDraw || !texture.IsValid())
            return;
        UiImageOpts o = opts;
        if (o.fit == UiImageFit::Slice &&
            (o.sliceMargins.x == 8.f && o.sliceMargins.y == 8.f))
            o.sliceMargins = mStyle.sliceMargins;
        mDraw->Image(rect, texture, o);
    }

    void UiContext::ModelPreview(std::string_view id, TextureHandle texture,
                                 glm::vec2 size, UiModelPreview* orbitTarget)
    {
        const UiId   sid   = HashId(id);
        const UiRect r     = Place(size);
        mLastId            = sid;
        mLastRect          = r;
        const bool hovered = Hit(r);
        mLastHovered       = hovered;

        if (orbitTarget)
        {
            orbitTarget->SetFrameDelta(mDt);
            auto& orbit = orbitTarget->Orbit();
            if (orbit.enabled)
            {
                const int btn = static_cast<int>(orbit.dragButton);
                if (hovered && mMouseClicked[btn])
                {
                    mActiveId = sid;
                    orbitTarget->FeedMouseButton(orbit.dragButton, true);
                    mWantMouse = true;
                }
                if (mActiveId == sid)
                {
                    mWantMouse   = true;
                    mMouseCursor = UiMouseCursor::Hand;
                    // Absolute mouse moves: approximate deltas from logical pos
                    // via previous frame storage on WidgetState.
                    auto& st = State(sid);
                    if (st.dragging || mMouseDown[btn])
                    {
                        if (!st.dragging)
                        {
                            st.dragging = true;
                            st.popupPos = { mMouseLogicalX, mMouseLogicalY };
                        }
                        const float dx = mMouseLogicalX - st.popupPos.x;
                        const float dy = mMouseLogicalY - st.popupPos.y;
                        if (dx != 0.f || dy != 0.f)
                            orbitTarget->FeedMouseMove(dx, dy);
                        st.popupPos = { mMouseLogicalX, mMouseLogicalY };
                    }
                    if (mMouseReleased[btn])
                    {
                        orbitTarget->FeedMouseButton(orbit.dragButton, false);
                        st.dragging = false;
                        mActiveId   = 0;
                    }
                }
            }
        }

        if (mDraw)
        {
            mDraw->Rect(
                r, mStyle.Color(UiCol::FrameBg), mStyle.Var(UiVar::Rounding),
                mStyle.Var(UiVar::BorderWidth), mStyle.Color(UiCol::Border));
            if (texture.IsValid())
            {
                const float pad = 2.f;
                UiImageOpts opts {};
                opts.rounding = mStyle.Var(UiVar::Rounding);
                mDraw->Image(
                    { r.x + pad, r.y + pad, r.w - pad * 2.f, r.h - pad * 2.f },
                    texture, opts);
            }
            if (mFocusId == sid)
                DrawFocusRing(r);
        }

        mLastActive = mActiveId == sid;
        mLastClicked =
            hovered && mMouseClicked[static_cast<int>(MouseButton::Left)];
        RegisterFocusable(sid, r);
    }

    bool UiContext::BeginPanel(std::string_view id, glm::vec2 size,
                               const UiPanelOpts& opts)
    {
        (void) id;
        const UiRect r = Place(size);
        mLastRect      = r;
        if (mDraw)
        {
            if (opts.bg.IsValid())
            {
                UiImageOpts o {};
                o.fit          = opts.fit;
                o.tint         = opts.tint;
                o.sliceMargins = mStyle.sliceMargins;
                mDraw->Image(r, opts.bg, o);
            }
            else
            {
                mDraw->Rect(r, mStyle.Color(UiCol::PanelBg) * opts.tint,
                            mStyle.Var(UiVar::Rounding),
                            mStyle.Var(UiVar::BorderWidth),
                            mStyle.Color(UiCol::Border));
            }
        }
        const float pad = mStyle.Var(UiVar::WindowPadding);
        PushParent({ r.x + pad, r.y + pad, r.w - pad * 2.f, r.h - pad * 2.f });
        return true;
    }

    void UiContext::EndPanel()
    {
        PopParent();
    }

    bool UiContext::BeginModal(std::string_view id, glm::vec2 size,
                               const UiPanelOpts& opts)
    {
        const UiId mid = HashId(id);
        (void) mid;
        if (mDraw)
        {
            mDraw->Rect({ 0, 0, mLogicalSize.x, mLogicalSize.y },
                        mStyle.Color(UiCol::ModalDim));
        }
        const UiRect centered { (mLogicalSize.x - size.x) * 0.5f,
                                (mLogicalSize.y - size.y) * 0.5f, size.x,
                                size.y };
        // Temporarily place without advancing root cursor
        if (mParents.empty())
            PushParent({ 0, 0, mLogicalSize.x, mLogicalSize.y });
        auto&       p  = mParents.back();
        const float cx = p.cursorX;
        const float cy = p.cursorY;
        p.cursorX      = centered.x;
        p.cursorY      = centered.y;
        UiPanelOpts o  = opts;
        o.modalDim     = true;
        BeginPanel(id, size, o);
        p.cursorX = cx;
        p.cursorY = cy;
        ++mModalLayer;
        mWantMouse = mWantKeyboard = mWantGamepad = true;
        if (mCloseTopModal)
        {
            mCloseTopModal = false;
            // stay open this frame; app checks separately via Escape
        }
        return true;
    }

    void UiContext::EndModal()
    {
        EndPanel();
        if (mModalLayer > 0)
            --mModalLayer;
    }

    void UiContext::Label(std::string_view text, float fontSize,
                          const glm::vec4* color)
    {
        const float fs =
            fontSize > 0.f ? fontSize : mStyle.Var(UiVar::FontSize);
        const UiRect r = Place({ 200.f, fs });
        mLastRect      = r;
        if (!mDraw || !mStyle.font)
            return;
        const auto& c = color ? *color : mStyle.Color(UiCol::Text);
        mDraw->Text({ r.x, r.y, r.w, r.h }, text, *mStyle.font, fs, c, 0.f,
                    1.5f, mStyle.Color(UiCol::TextOutline));
    }

    void UiContext::TextWrapped(std::string_view text, float maxWidth,
                                float fontSize)
    {
        const float fs =
            fontSize > 0.f ? fontSize : mStyle.Var(UiVar::FontSize);
        // Approximate height: one line; wrap handled in UiDraw::Text
        const float lines =
            maxWidth > 0.f
                ? std::max(1.f, std::ceil(static_cast<float>(text.size()) * fs *
                                          0.5f / maxWidth))
                : 1.f;
        const UiRect r =
            Place({ maxWidth > 0.f ? maxWidth : 400.f, fs * lines });
        mLastRect = r;
        if (!mDraw || !mStyle.font)
            return;
        mDraw->Text(r, text, *mStyle.font, fs, mStyle.Color(UiCol::Text),
                    maxWidth, 1.5f, mStyle.Color(UiCol::TextOutline));
    }

    void UiContext::ProgressBar(float fill01, glm::vec2 size)
    {
        const UiRect r = Place(size);
        mLastRect      = r;
        if (!mDraw)
            return;
        mDraw->ProgressBar(r, fill01, mStyle.Color(UiCol::FrameBg),
                           mStyle.Color(UiCol::ProgressFill),
                           mStyle.Var(UiVar::Rounding));
    }

    void UiContext::Separator()
    {
        const UiRect r = Place({ CurrentParent().w, 2.f });
        if (mDraw)
            mDraw->Rect(r, mStyle.Color(UiCol::Border));
    }

    bool UiContext::Button(std::string_view label, glm::vec2 size)
    {
        const UiId   id = HashId(label);
        const UiRect r  = Place(size);
        mLastRect       = r;
        mLastId         = id;
        RegisterFocusable(id, r);
        const bool hovered = Hit(r);
        mLastHovered       = hovered;
        if (hovered)
        {
            mWantMouse   = true;
            mMouseCursor = UiMouseCursor::Hand;
        }
        if (hovered && mMouseClicked[static_cast<int>(MouseButton::Left)])
            mActiveId = id;
        const bool clicked =
            hovered && mActiveId == id &&
            mMouseReleased[static_cast<int>(MouseButton::Left)];
        mLastClicked = clicked;
        mLastActive  = mActiveId == id;
        if (clicked)
            mActiveId = 0;

        glm::vec4 col = mStyle.Color(UiCol::Button);
        if (mActiveId == id)
            col = mStyle.Color(UiCol::ButtonActive);
        else if (hovered || mFocusId == id)
            col = mStyle.Color(UiCol::ButtonHovered);

        if (mDraw)
        {
            TextureHandle slice = mStyle.buttonSlice;
            if (slice.IsValid())
            {
                UiImageOpts o {};
                o.fit          = UiImageFit::Slice;
                o.tint         = col;
                o.sliceMargins = mStyle.sliceMargins;
                mDraw->Image(r, slice, o);
            }
            else
            {
                mDraw->Rect(r, col, mStyle.Var(UiVar::Rounding),
                            mStyle.Var(UiVar::BorderWidth),
                            mStyle.Color(UiCol::Border));
            }
            if (mStyle.font)
            {
                const float fs = mStyle.Var(UiVar::FontSizeSmall);
                mDraw->Text({ r.x + mStyle.Var(UiVar::FramePadding),
                              r.y + (r.h - fs) * 0.5f, r.w, fs },
                            label, *mStyle.font, fs, mStyle.Color(UiCol::Text),
                            0.f, 1.f, mStyle.Color(UiCol::TextOutline));
            }
            if (mFocusId == id)
                DrawFocusRing(r);
        }
        return clicked;
    }

    bool UiContext::Checkbox(std::string_view label, bool* value)
    {
        if (!value)
            return false;
        const float  box = 22.f;
        const UiId   id  = HashId(label);
        const UiRect r   = Place({ 200.f, box });
        const UiRect boxR { r.x, r.y, box, box };
        mLastRect = r;
        RegisterFocusable(id, boxR);
        const bool hovered = Hit(boxR);
        mLastHovered       = hovered;
        if (hovered)
            mWantMouse = true;
        bool changed = false;
        if (hovered && mMouseClicked[static_cast<int>(MouseButton::Left)])
        {
            *value  = !*value;
            changed = true;
        }
        if (mDraw)
        {
            mDraw->Rect(boxR, mStyle.Color(UiCol::FrameBg), 4.f, 1.f,
                        mStyle.Color(UiCol::Border));
            if (*value)
                mDraw->Rect({ boxR.x + 4, boxR.y + 4, box - 8, box - 8 },
                            mStyle.Color(UiCol::CheckMark), 2.f);
            if (mStyle.font)
                mDraw->Text({ boxR.x + box + 8, boxR.y, 180.f, box }, label,
                            *mStyle.font, mStyle.Var(UiVar::FontSizeSmall),
                            mStyle.Color(UiCol::Text));
        }
        return changed;
    }

    bool UiContext::SliderFloat(std::string_view label, float* value,
                                float vMin, float vMax, glm::vec2 size)
    {
        if (!value)
            return false;
        Label(label, mStyle.Var(UiVar::FontSizeSmall));
        const UiId   id = HashId(label);
        const UiRect r  = Place(size);
        mLastRect       = r;
        RegisterFocusable(id, r);
        const bool hovered = Hit(r);
        if (hovered)
            mWantMouse = true;
        bool changed = false;
        if ((hovered || mActiveId == id) &&
            mMouseDown[static_cast<int>(MouseButton::Left)])
        {
            mActiveId     = id;
            const float t = std::clamp(
                (mMouseLogicalX - r.x) / std::max(r.w, 1.f), 0.f, 1.f);
            const float nv = vMin + t * (vMax - vMin);
            if (nv != *value)
            {
                *value  = nv;
                changed = true;
            }
        }
        if (mMouseReleased[static_cast<int>(MouseButton::Left)] &&
            mActiveId == id)
            mActiveId = 0;
        if (mDraw)
        {
            mDraw->Rect(r, mStyle.Color(UiCol::FrameBg),
                        mStyle.Var(UiVar::Rounding));
            const float t = std::clamp(
                (*value - vMin) / std::max(vMax - vMin, 1e-5f), 0.f, 1.f);
            mDraw->Rect({ r.x + t * r.w - 6.f, r.y - 2.f, 12.f, r.h + 4.f },
                        mStyle.Color(UiCol::SliderGrab), 4.f);
        }
        return changed;
    }

    bool UiContext::Selectable(std::string_view label, bool selected,
                               glm::vec2 size)
    {
        if (size.x <= 0.f)
            size.x = CurrentParent().w;
        if (size.y <= 0.f)
            size.y = 28.f;
        const UiId   id = HashId(label);
        const UiRect r  = Place(size);
        mLastRect       = r;
        RegisterFocusable(id, r);
        const bool hovered = Hit(r);
        mLastHovered       = hovered;
        if (hovered)
            mWantMouse = true;
        const bool clicked =
            hovered && mMouseClicked[static_cast<int>(MouseButton::Left)];
        mLastClicked = clicked;
        if (mDraw)
        {
            if (selected || hovered)
                mDraw->Rect(r, mStyle.Color(UiCol::ListSelected),
                            mStyle.Var(UiVar::Rounding));
            if (mStyle.font)
                mDraw->Text({ r.x + 8, r.y + 4, r.w - 8, r.h }, label,
                            *mStyle.font, mStyle.Var(UiVar::FontSizeSmall),
                            mStyle.Color(UiCol::Text));
        }
        return clicked;
    }

    bool UiContext::BeginScrollView(std::string_view id, glm::vec2 size,
                                    float contentHeight)
    {
        const UiId   sid = HashId(id);
        const UiRect r   = Place(size);
        auto&        st  = State(sid);
        if (Hit(r))
        {
            mWantMouse = true;
            st.scrollY -= mWheel * 40.f;
        }
        const float maxScroll = std::max(0.f, contentHeight - size.y);
        st.scrollY            = std::clamp(st.scrollY, 0.f, maxScroll);
        mScrolls.push_back({ sid, r, contentHeight });
        PushParent({ r.x, r.y - st.scrollY, r.w, contentHeight });
        return true;
    }

    void UiContext::EndScrollView()
    {
        PopParent();
        if (!mScrolls.empty())
            mScrolls.pop_back();
    }

    void UiContext::SetScrollHereY(float ratio)
    {
        if (mScrolls.empty())
            return;
        auto&       sc        = mScrolls.back();
        auto&       st        = State(sc.id);
        const float maxScroll = std::max(0.f, sc.contentH - sc.view.h);
        st.scrollY            = std::clamp(ratio, 0.f, 1.f) * maxScroll;
    }

    bool UiContext::BeginList(std::string_view id, glm::vec2 size,
                              int itemCount, float itemHeight, int* scrollIndex)
    {
        const UiId sid = HashId(id);
        BeginScrollView(id, size, itemCount * itemHeight);
        auto&   st = State(sid);
        ListCtx ctx {};
        ctx.itemHeight   = itemHeight;
        ctx.itemCount    = itemCount;
        ctx.area         = mScrolls.back().view;
        ctx.firstVisible = static_cast<int>(st.scrollY / itemHeight);
        ctx.visibleCount = static_cast<int>(std::ceil(size.y / itemHeight)) + 1;
        if (scrollIndex)
            *scrollIndex = ctx.firstVisible;
        mLists.push_back(ctx);
        return true;
    }

    bool UiContext::ListItem(int index, std::string_view label, bool selected)
    {
        if (mLists.empty())
            return false;
        const auto& ctx = mLists.back();
        if (index < ctx.firstVisible ||
            index >= ctx.firstVisible + ctx.visibleCount)
            return false;
        return Selectable(label, selected, { 0.f, ctx.itemHeight });
    }

    void UiContext::EndList()
    {
        if (!mLists.empty())
            mLists.pop_back();
        EndScrollView();
    }

    bool UiContext::BeginGrid(std::string_view id, int cols, glm::vec2 cellSize,
                              float gap)
    {
        (void) id;
        const float w = cols * cellSize.x + std::max(0, cols - 1) * gap;
        // height grows with content — use parent remaining
        const UiRect area = Place({ w, CurrentParent().h });
        // rewind cursor for grid cells
        if (!mParents.empty())
        {
            mParents.back().cursorY = area.y;
            mParents.back().cursorX = area.x;
        }
        mGrids.push_back({ cols, cellSize, gap, 0, area });
        return true;
    }

    void UiContext::EndGrid()
    {
        if (mGrids.empty())
            return;
        auto&     g    = mGrids.back();
        const int rows = (g.index + g.cols - 1) / std::max(1, g.cols);
        if (!mParents.empty())
        {
            mParents.back().cursorY = g.area.y + rows * (g.cell.y + g.gap);
            mParents.back().cursorX = g.area.x;
        }
        mGrids.pop_back();
    }

    bool UiContext::BeginTabBar(std::string_view id)
    {
        const UiId tid = HashId(id);
        mTabs.push_back({ tid, State(tid).activeTab, 0 });
        return true;
    }

    bool UiContext::Tab(std::string_view label)
    {
        if (mTabs.empty())
            return false;
        auto&      tab      = mTabs.back();
        const int  i        = tab.drawn++;
        const bool selected = (i == tab.index);
        if (Button(label, { 100.f, 32.f }))
        {
            tab.index               = i;
            State(tab.id).activeTab = i;
        }
        return selected || State(tab.id).activeTab == i;
    }

    void UiContext::EndTabBar()
    {
        if (!mTabs.empty())
            mTabs.pop_back();
    }

    bool UiContext::ItemSlot(std::string_view id, TextureHandle icon,
                             int stackCount, bool selected, glm::vec2 size)
    {
        const UiId   sid = HashId(id);
        const UiRect r   = Place(size);
        mLastRect        = r;
        mLastId          = sid;
        RegisterFocusable(sid, r);
        const bool hovered = Hit(r);
        mLastHovered       = hovered;
        if (hovered)
        {
            mWantMouse = true;
            if (!mDrag.active)
                mMouseCursor = UiMouseCursor::Hand;
            State(sid).hoverTime += mDt;
        }
        else
            State(sid).hoverTime = 0.f;

        if (hovered && mMouseClicked[static_cast<int>(MouseButton::Left)])
            mActiveId = sid;
        if (mActiveId == sid &&
            mMouseDown[static_cast<int>(MouseButton::Left)] &&
            (std::abs(mMouseLogicalX - r.Center().x) > 4.f ||
             std::abs(mMouseLogicalY - r.Center().y) > 4.f))
            State(sid).dragging = true;

        const bool clicked =
            hovered && mMouseReleased[static_cast<int>(MouseButton::Left)] &&
            !State(sid).dragging;
        if (mMouseReleased[static_cast<int>(MouseButton::Left)] &&
            mActiveId == sid)
        {
            State(sid).dragging = false;
            mActiveId           = 0;
        }
        mLastClicked = clicked;

        const bool isDragSource = mDrag.active && State(sid).dragging;
        if (mDrag.active && hovered)
            mMouseCursor = UiMouseCursor::Move;

        if (mDraw)
        {
            glm::vec4 bg = mStyle.Color(UiCol::FrameBg);
            if (selected || hovered || (mDrag.active && hovered))
                bg = mStyle.Color(UiCol::ListSelected);
            if (isDragSource)
                bg.a *= 0.45f;
            mDraw->Rect(r, bg, 4.f, 1.f, mStyle.Color(UiCol::Border));
            if (icon.IsValid())
            {
                const float pad = 4.f;
                UiImageOpts opts {};
                if (isDragSource)
                    opts.tint = { 1.f, 1.f, 1.f, 0.35f };
                mDraw->Image(
                    { r.x + pad, r.y + pad, r.w - pad * 2, r.h - pad * 2 },
                    icon, opts);
            }
            if (stackCount > 1)
                IconBadge(std::to_string(stackCount), r);
            if (mFocusId == sid)
                DrawFocusRing(r);
        }
        return clicked;
    }

    void UiContext::IconBadge(std::string_view text, const UiRect& slot)
    {
        if (!mDraw || !mStyle.font)
            return;
        const float fs = 14.f;
        mDraw->Text(
            { slot.x + slot.w - 18.f, slot.y + slot.h - fs - 2.f, 20.f, fs },
            text, *mStyle.font, fs, mStyle.Color(UiCol::Text), 0.f, 1.f,
            mStyle.Color(UiCol::TextOutline));
    }

    bool UiContext::AbilitySlot(std::string_view id, TextureHandle icon,
                                std::string_view hotkey,
                                float cooldownRemaining01, glm::vec2 size)
    {
        const UiId   sid = HashId(id);
        const UiRect r   = Place(size);
        mLastRect        = r;
        mLastId          = sid;
        RegisterFocusable(sid, r);
        const bool hovered = Hit(r);
        mLastHovered       = hovered;
        const float rem    = std::clamp(cooldownRemaining01, 0.f, 1.f);
        const bool  ready  = rem <= 1e-3f;
        if (hovered)
        {
            mWantMouse = true;
            if (ready)
                mMouseCursor = UiMouseCursor::Hand;
            State(sid).hoverTime += mDt;
        }
        else
            State(sid).hoverTime = 0.f;

        if (hovered && ready &&
            mMouseClicked[static_cast<int>(MouseButton::Left)])
            mActiveId = sid;
        const bool clicked =
            ready && hovered && mActiveId == sid &&
            mMouseReleased[static_cast<int>(MouseButton::Left)];
        mLastClicked = clicked;
        mLastActive  = mActiveId == sid;
        if (clicked)
            mActiveId = 0;
        if (mMouseReleased[static_cast<int>(MouseButton::Left)] &&
            mActiveId == sid)
            mActiveId = 0;

        if (mDraw)
        {
            glm::vec4 bg = mStyle.Color(UiCol::FrameBg);
            if (hovered && ready)
                bg = mStyle.Color(UiCol::ListSelected);
            mDraw->Rect(r, bg, 6.f, 1.f, mStyle.Color(UiCol::Border));
            if (icon.IsValid())
            {
                const float pad = 4.f;
                UiImageOpts opts {};
                if (!ready)
                    opts.tint = { 0.55f, 0.55f, 0.55f, 1.f };
                mDraw->Image(
                    { r.x + pad, r.y + pad, r.w - pad * 2.f, r.h - pad * 2.f },
                    icon, opts);
            }
            if (rem > 1e-3f)
                mDraw->CooldownRadial(r, rem, { 0.02f, 0.03f, 0.05f, 0.72f },
                                      6.f);
            if (!hotkey.empty() && mStyle.font)
            {
                const float fs = 13.f;
                mDraw->Rect(
                    { r.x + 3.f, r.y + r.h - fs - 6.f, fs + 6.f, fs + 4.f },
                    { 0.05f, 0.05f, 0.06f, 0.85f }, 3.f);
                mDraw->Text({ r.x + 5.f, r.y + r.h - fs - 4.f, 24.f, fs },
                            hotkey, *mStyle.font, fs, mStyle.Color(UiCol::Text),
                            0.f, 1.f, mStyle.Color(UiCol::TextOutline));
            }
            if (mFocusId == sid)
                DrawFocusRing(r);
        }
        return clicked;
    }

    bool UiContext::BeginTooltip(std::string_view id, float delay)
    {
        (void) id;
        if (!mLastHovered || State(mLastId).hoverTime < delay)
            return false;
        mInTooltip = true;
        ++mOverlayDepth;
        if (mDraw)
            mDraw->BeginOverlay();

        constexpr float kW   = 240.f;
        constexpr float kH   = 110.f;
        constexpr float kPad = 12.f;
        const float     minX = mSafeArea.x;
        const float     minY = mSafeArea.y;
        const float     maxX = mLogicalSize.x - mSafeArea.w - kW;
        const float     maxY = mLogicalSize.y - mSafeArea.h - kH;

        // Prefer below-right; flip above / left when near screen edges.
        float x = mMouseLogicalX + kPad;
        float y = mMouseLogicalY + kPad;
        if (y + kH > mLogicalSize.y - mSafeArea.h)
            y = mMouseLogicalY - kPad - kH;
        if (x + kW > mLogicalSize.x - mSafeArea.w)
            x = mMouseLogicalX - kPad - kW;
        x = std::clamp(x, minX, std::max(minX, maxX));
        y = std::clamp(y, minY, std::max(minY, maxY));

        const UiRect tip { x, y, kW, kH };
        if (mDraw)
            mDraw->Rect(tip, mStyle.Color(UiCol::PanelBg), 4.f, 1.f,
                        mStyle.Color(UiCol::Border));
        PushParent({ tip.x + 8, tip.y + 8, tip.w - 16, tip.h - 16 });
        return true;
    }

    void UiContext::EndTooltip()
    {
        if (mInTooltip)
        {
            PopParent();
            mInTooltip = false;
            if (mOverlayDepth > 0)
                --mOverlayDepth;
            if (mDraw)
                mDraw->EndOverlay();
        }
    }

    bool UiContext::BeginPopupContextItem(std::string_view id)
    {
        const UiId pid = HashId(id);
        auto&      st  = State(pid);
        if (mLastHovered && mMouseClicked[static_cast<int>(MouseButton::Right)])
        {
            if (mPopupId != 0 && mPopupId != pid)
                State(mPopupId).open = false;
            mPopupOpen         = true;
            mPopupId           = pid;
            st.open            = true;
            constexpr float kW = 160.f;
            constexpr float kH = 120.f;
            float           x  = mLastRect.x + mLastRect.w + 4.f;
            float           y  = mLastRect.y;
            if (x + kW > mLogicalSize.x - mSafeArea.w)
                x = mLastRect.x - kW - 4.f;
            if (y + kH > mLogicalSize.y - mSafeArea.h)
                y = mLogicalSize.y - mSafeArea.h - kH;
            x = std::clamp(
                x, mSafeArea.x,
                std::max(mSafeArea.x, mLogicalSize.x - mSafeArea.w - kW));
            y = std::clamp(
                y, mSafeArea.y,
                std::max(mSafeArea.y, mLogicalSize.y - mSafeArea.h - kH));
            st.popupPos = { x, y };
        }
        if (!st.open)
            return false;
        mPopupOpen = true;
        mPopupId   = pid;
        ++mOverlayDepth;
        if (mDraw)
            mDraw->BeginOverlay();
        constexpr float kW = 160.f;
        constexpr float kH = 120.f;
        const UiRect    tip { st.popupPos.x, st.popupPos.y, kW, kH };
        st.popupRect = tip;
        if (mDraw)
            mDraw->Rect(tip, mStyle.Color(UiCol::PanelBg), 4.f, 1.f,
                        mStyle.Color(UiCol::Border));
        PushParent({ tip.x + 4, tip.y + 4, tip.w - 8, tip.h - 8 });
        mWantMouse   = true;
        mMouseCursor = UiMouseCursor::Hand;
        return true;
    }

    void UiContext::EndPopup()
    {
        PopParent();
        if (mOverlayDepth > 0)
            --mOverlayDepth;
        if (mDraw)
            mDraw->EndOverlay();
        if (mPopupId == 0)
            return;
        auto&      st = State(mPopupId);
        const bool clickOutside =
            mMouseClicked[static_cast<int>(MouseButton::Left)] &&
            !Hit(st.popupRect);
        if (mLastClicked || clickOutside)
        {
            st.open    = false;
            mPopupOpen = false;
            mPopupId   = 0;
        }
    }

    bool UiContext::BeginDragDropSource(
        std::string_view type, std::uint64_t payload, TextureHandle preview)
    {
        if (mLastId == 0)
            return false;
        if (!State(mLastId).dragging && mActiveId != mLastId)
            return false;
        if (!State(mLastId).dragging)
            return false;
        mDrag.type    = std::string(type);
        mDrag.value   = payload;
        mDrag.active  = true;
        mDrag.preview = preview;
        mMouseCursor  = UiMouseCursor::Move;
        return true;
    }

    bool UiContext::AcceptDragDropPayload(std::string_view type,
                                          std::uint64_t*   outPayload)
    {
        if (!mDrag.active || mDrag.type != type)
            return false;
        if (!mLastHovered)
            return false;
        if (!mMouseReleased[static_cast<int>(MouseButton::Left)])
            return false;
        if (outPayload)
            *outPayload = mDrag.value;
        mDrag.active = false;
        return true;
    }

    void UiContext::FocusTextInput(std::string_view id, std::string_view text)
    {
        mPendingTextFocusId = HashId(id);
        mPendingTextFocus   = true;
        mTextEditBuffer     = std::string(text);
        mTextInputId        = mPendingTextFocusId;
        mFocusId            = mPendingTextFocusId;
        mWantTextInput      = true;
    }

    bool UiContext::TextInput(std::string_view id, std::string& buffer,
                              std::size_t maxLen, glm::vec2 size)
    {
        const UiId   tid = HashId(id);
        const UiRect r   = Place(size);
        mLastRect        = r;
        RegisterFocusable(tid, r);
        mTextInputSeen = true;
        if (mPendingTextFocus && mPendingTextFocusId == tid)
        {
            mTextInputId    = tid;
            mFocusId        = tid;
            mTextEditBuffer = buffer.empty() ? mTextEditBuffer : buffer;
            if (!mTextEditBuffer.empty())
                buffer = mTextEditBuffer;
            mPendingTextFocus = false;
        }
        const bool hovered = Hit(r);
        if (hovered)
            mWantMouse = true;
        if (hovered && mMouseClicked[static_cast<int>(MouseButton::Left)])
        {
            mTextInputId    = tid;
            mTextEditBuffer = buffer;
            mFocusId        = tid;
        }
        else if (mMouseClicked[static_cast<int>(MouseButton::Left)] &&
                 !hovered && mTextInputId == tid)
        {
            mTextInputId = 0;
        }
        const bool focused = mTextInputId == tid;
        if (focused)
        {
            mWantTextInput = true;
            mWantKeyboard  = true;
            buffer         = mTextEditBuffer;
            if (buffer.size() > maxLen)
                buffer.resize(maxLen);
            mTextEditBuffer = buffer;
        }
        bool submitted = false;
        if (focused && mTextSubmit)
        {
            submitted   = true;
            mTextSubmit = false;
        }
        if (mDraw)
        {
            mDraw->Rect(r, mStyle.Color(UiCol::FrameBg), 4.f, 1.f,
                        focused ? mStyle.Color(UiCol::FocusRing)
                                : mStyle.Color(UiCol::Border));
            if (mStyle.font)
            {
                std::string display = buffer;
                if (focused)
                    display.push_back('|');
                mDraw->Text({ r.x + 6, r.y + 4, r.w - 12, r.h }, display,
                            *mStyle.font, mStyle.Var(UiVar::FontSizeSmall),
                            mStyle.Color(UiCol::Text));
            }
        }
        return submitted;
    }

    bool UiContext::BeginColumns(std::string_view id, int count, float widths[])
    {
        (void) id;
        count = std::clamp(count, 1, 8);
        ColCtx c {};
        c.count  = count;
        c.index  = 0;
        c.area   = CurrentParent();
        c.startY = mParents.empty() ? 0.f : mParents.back().cursorY;
        for (int i = 0; i < count; ++i)
            c.widths[i] = widths ? widths[i] : c.area.w / count;
        mColumns.push_back(c);
        return true;
    }

    void UiContext::NextColumn()
    {
        if (mColumns.empty())
            return;
        auto& c = mColumns.back();
        c.index = std::min(c.index + 1, c.count - 1);
        if (!mParents.empty())
            mParents.back().cursorY = c.startY;
    }

    void UiContext::EndColumns()
    {
        if (!mColumns.empty())
            mColumns.pop_back();
    }

} // namespace FREYA_NAMESPACE
