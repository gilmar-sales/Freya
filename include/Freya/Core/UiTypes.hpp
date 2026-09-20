#pragma once

#include "Freya/Asset/TexturePool.hpp"

#include <algorithm>
#include <cstdint>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    using UiId = std::uint32_t;

    enum class UiMouseCursor : std::uint32_t
    {
        Arrow = 0,
        Hand,
        Move,
        NotAllowed,
    };

    enum class UiImageFit : std::uint32_t
    {
        Stretch = 0,
        Contain = 1,
        Cover   = 2,
        Slice   = 3,
        Tile    = 4,
    };

    enum class UiAnchor : std::uint32_t
    {
        TopLeft = 0,
        Top,
        TopRight,
        Left,
        Center,
        Right,
        BottomLeft,
        Bottom,
        BottomRight,
    };

    enum class UiCol : std::uint32_t
    {
        WindowBg = 0,
        PanelBg,
        ModalDim,
        Button,
        ButtonHovered,
        ButtonActive,
        ButtonDisabled,
        Text,
        TextDisabled,
        TextOutline,
        Border,
        FrameBg,
        SliderGrab,
        ProgressFill,
        CheckMark,
        ListSelected,
        FocusRing,
        COUNT
    };

    enum class UiVar : std::uint32_t
    {
        WindowPadding = 0,
        FramePadding,
        ItemSpacing,
        Indent,
        ScrollBarSize,
        BorderWidth,
        Rounding,
        FocusRingWidth,
        FontSize,
        FontSizeSmall,
        FontSizeTitle,
        COUNT
    };

    struct UiRect
    {
        float x = 0.f;
        float y = 0.f;
        float w = 0.f;
        float h = 0.f;

        [[nodiscard]] bool Contains(float px, float py) const
        {
            return px >= x && py >= y && px < x + w && py < y + h;
        }

        [[nodiscard]] UiRect Expand(float pad) const
        {
            return { x - pad, y - pad, w + pad * 2.f, h + pad * 2.f };
        }

        [[nodiscard]] UiRect Intersect(const UiRect& o) const
        {
            const float x0 = std::max(x, o.x);
            const float y0 = std::max(y, o.y);
            const float x1 = std::min(x + w, o.x + o.w);
            const float y1 = std::min(y + h, o.y + o.h);
            return { x0, y0, std::max(0.f, x1 - x0), std::max(0.f, y1 - y0) };
        }

        [[nodiscard]] glm::vec2 Center() const
        {
            return { x + w * 0.5f, y + h * 0.5f };
        }
    };

    struct UiImageOpts
    {
        glm::vec4   uvRect { 0.f, 0.f, 1.f, 1.f };
        glm::vec4   tint { 1.f };
        UiImageFit  fit = UiImageFit::Stretch;
        float       rounding = 0.f;
        glm::vec4   sliceMargins { 8.f, 8.f, 8.f, 8.f }; // L R T B px
        glm::vec2   sourceSize { 0.f }; // for Contain/Cover; 0 = use uv
    };

    struct UiPanelOpts
    {
        TextureHandle bg {};
        UiImageFit    fit  = UiImageFit::Slice;
        glm::vec4     tint { 1.f };
        bool          modalDim = false;
    };

} // namespace FREYA_NAMESPACE
