#include "Freya/Core/UiStyle.hpp"

#include <charconv>
#include <sstream>

namespace FREYA_NAMESPACE
{
    namespace
    {
        const char* ColName(UiCol c)
        {
            switch (c)
            {
                case UiCol::WindowBg:
                    return "WindowBg";
                case UiCol::PanelBg:
                    return "PanelBg";
                case UiCol::ModalDim:
                    return "ModalDim";
                case UiCol::Button:
                    return "Button";
                case UiCol::ButtonHovered:
                    return "ButtonHovered";
                case UiCol::ButtonActive:
                    return "ButtonActive";
                case UiCol::ButtonDisabled:
                    return "ButtonDisabled";
                case UiCol::Text:
                    return "Text";
                case UiCol::TextDisabled:
                    return "TextDisabled";
                case UiCol::TextOutline:
                    return "TextOutline";
                case UiCol::Border:
                    return "Border";
                case UiCol::FrameBg:
                    return "FrameBg";
                case UiCol::SliderGrab:
                    return "SliderGrab";
                case UiCol::ProgressFill:
                    return "ProgressFill";
                case UiCol::CheckMark:
                    return "CheckMark";
                case UiCol::ListSelected:
                    return "ListSelected";
                case UiCol::FocusRing:
                    return "FocusRing";
                case UiCol::ToastBg:
                    return "ToastBg";
                case UiCol::HeaderBg:
                    return "HeaderBg";
                case UiCol::COUNT:
                    return "?";
            }
            return "?";
        }

        const char* VarName(UiVar v)
        {
            switch (v)
            {
                case UiVar::WindowPadding:
                    return "WindowPadding";
                case UiVar::FramePadding:
                    return "FramePadding";
                case UiVar::ItemSpacing:
                    return "ItemSpacing";
                case UiVar::Indent:
                    return "Indent";
                case UiVar::ScrollBarSize:
                    return "ScrollBarSize";
                case UiVar::BorderWidth:
                    return "BorderWidth";
                case UiVar::Rounding:
                    return "Rounding";
                case UiVar::FocusRingWidth:
                    return "FocusRingWidth";
                case UiVar::FontSize:
                    return "FontSize";
                case UiVar::FontSizeSmall:
                    return "FontSizeSmall";
                case UiVar::FontSizeTitle:
                    return "FontSizeTitle";
                case UiVar::DisabledAlpha:
                    return "DisabledAlpha";
                case UiVar::AnimSpeed:
                    return "AnimSpeed";
                case UiVar::COUNT:
                    return "?";
            }
            return "?";
        }
    } // namespace
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
        s.Color(UiCol::ToastBg)        = { 0.08f, 0.09f, 0.10f, 0.95f };
        s.Color(UiCol::HeaderBg)       = { 0.16f, 0.17f, 0.18f, 1.f };

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
        s.Var(UiVar::DisabledAlpha)  = 0.45f;
        s.Var(UiVar::AnimSpeed)      = 10.f;
        return s;
    }

    std::string UiStyle::SaveIni() const
    {
        std::ostringstream out;
        for (std::size_t i = 0; i < static_cast<std::size_t>(UiCol::COUNT); ++i)
        {
            const auto& c = colors[i];
            out << "Color." << ColName(static_cast<UiCol>(i)) << '=' << c.r
                << ' ' << c.g << ' ' << c.b << ' ' << c.a << '\n';
        }
        for (std::size_t i = 0; i < static_cast<std::size_t>(UiVar::COUNT); ++i)
            out << "Var." << VarName(static_cast<UiVar>(i)) << '=' << vars[i]
                << '\n';
        return out.str();
    }

    bool UiStyle::LoadIni(std::string_view ini)
    {
        bool               ok = true;
        std::string        text(ini);
        std::istringstream in(text);
        std::string        line;
        while (std::getline(in, line))
        {
            if (line.empty() || line[0] == '#' || line[0] == ';')
                continue;
            const auto eq = line.find('=');
            if (eq == std::string::npos)
            {
                ok = false;
                continue;
            }
            const std::string  key = line.substr(0, eq);
            const std::string  val = line.substr(eq + 1);
            std::istringstream vs(val);
            if (key.rfind("Color.", 0) == 0)
            {
                const std::string name = key.substr(6);
                glm::vec4         c {};
                if (!(vs >> c.r >> c.g >> c.b >> c.a))
                {
                    ok = false;
                    continue;
                }
                bool found = false;
                for (std::size_t i = 0;
                     i < static_cast<std::size_t>(UiCol::COUNT);
                     ++i)
                {
                    if (name == ColName(static_cast<UiCol>(i)))
                    {
                        colors[i] = c;
                        found     = true;
                        break;
                    }
                }
                if (!found)
                    ok = false;
            }
            else if (key.rfind("Var.", 0) == 0)
            {
                const std::string name = key.substr(4);
                float             v {};
                if (!(vs >> v))
                {
                    ok = false;
                    continue;
                }
                bool found = false;
                for (std::size_t i = 0;
                     i < static_cast<std::size_t>(UiVar::COUNT);
                     ++i)
                {
                    if (name == VarName(static_cast<UiVar>(i)))
                    {
                        vars[i] = v;
                        found   = true;
                        break;
                    }
                }
                if (!found)
                    ok = false;
            }
            else
            {
                ok = false;
            }
        }
        return ok;
    }

} // namespace FREYA_NAMESPACE
