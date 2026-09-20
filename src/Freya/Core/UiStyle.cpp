#include "Freya/Core/UiStyle.hpp"

namespace FREYA_NAMESPACE
{
    UiStyle UiStyle::Default()
    {
        UiStyle s {};
        // Graphite + amber/copper accent (Freya default look).
        s.Color(UiCol::WindowBg)       = { 0.10f, 0.11f, 0.12f, 0.94f };
        s.Color(UiCol::PanelBg)        = { 0.14f, 0.15f, 0.16f, 0.96f };
        s.Color(UiCol::ModalDim)       = { 0.00f, 0.00f, 0.00f, 0.55f };
        s.Color(UiCol::Button)         = { 0.28f, 0.22f, 0.14f, 1.f };
        s.Color(UiCol::ButtonHovered)  = { 0.42f, 0.32f, 0.16f, 1.f };
        s.Color(UiCol::ButtonActive)   = { 0.55f, 0.40f, 0.18f, 1.f };
        s.Color(UiCol::ButtonDisabled) = { 0.20f, 0.20f, 0.21f, 0.6f };
        s.Color(UiCol::Text)           = { 0.92f, 0.90f, 0.86f, 1.f };
        s.Color(UiCol::TextDisabled)   = { 0.55f, 0.54f, 0.52f, 1.f };
        s.Color(UiCol::TextOutline)    = { 0.02f, 0.02f, 0.02f, 0.9f };
        s.Color(UiCol::Border)         = { 0.35f, 0.32f, 0.28f, 1.f };
        s.Color(UiCol::FrameBg)        = { 0.08f, 0.08f, 0.09f, 1.f };
        s.Color(UiCol::SliderGrab)     = { 0.72f, 0.52f, 0.22f, 1.f };
        s.Color(UiCol::ProgressFill)   = { 0.72f, 0.48f, 0.18f, 1.f };
        s.Color(UiCol::CheckMark)      = { 0.85f, 0.70f, 0.35f, 1.f };
        s.Color(UiCol::ListSelected)   = { 0.35f, 0.28f, 0.16f, 1.f };
        s.Color(UiCol::FocusRing)      = { 0.90f, 0.70f, 0.30f, 1.f };

        s.Var(UiVar::WindowPadding)  = 12.f;
        s.Var(UiVar::FramePadding)   = 8.f;
        s.Var(UiVar::ItemSpacing)    = 8.f;
        s.Var(UiVar::Indent)         = 16.f;
        s.Var(UiVar::ScrollBarSize)  = 12.f;
        s.Var(UiVar::BorderWidth)    = 1.f;
        s.Var(UiVar::Rounding)       = 6.f;
        s.Var(UiVar::FocusRingWidth) = 2.f;
        s.Var(UiVar::FontSize)       = 22.f;
        s.Var(UiVar::FontSizeSmall)  = 16.f;
        s.Var(UiVar::FontSizeTitle)  = 28.f;
        return s;
    }

} // namespace FREYA_NAMESPACE
