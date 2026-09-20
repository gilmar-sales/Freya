#include "Freya/Core/UiDraw.hpp"

#include "Freya/Asset/FontAtlas.hpp"

#include <algorithm>
#include <string_view>
#include <vector>

namespace FREYA_NAMESPACE
{
    namespace
    {
        bool NextCodepoint(std::string_view utf8, std::size_t& i, char32_t& cp)
        {
            if (i >= utf8.size())
                return false;
            const auto c = static_cast<unsigned char>(utf8[i]);
            if (c < 0x80)
            {
                cp = c;
                ++i;
                return true;
            }
            if ((c & 0xE0) == 0xC0 && i + 1 < utf8.size())
            {
                cp = (static_cast<char32_t>(c & 0x1F) << 6) |
                     (static_cast<unsigned char>(utf8[i + 1]) & 0x3F);
                i += 2;
                return true;
            }
            if ((c & 0xF0) == 0xE0 && i + 2 < utf8.size())
            {
                cp = (static_cast<char32_t>(c & 0x0F) << 12) |
                     (static_cast<char32_t>(
                          static_cast<unsigned char>(utf8[i + 1]) & 0x3F)
                      << 6) |
                     (static_cast<unsigned char>(utf8[i + 2]) & 0x3F);
                i += 3;
                return true;
            }
            ++i;
            cp = U'?';
            return true;
        }

        bool IsSpace(const char32_t cp)
        {
            return cp == U' ' || cp == U'\t';
        }
    } // namespace

    void UiDraw::Text(const UiRect& rect, std::string_view utf8,
                      const FontAtlas& font, const float heightPx,
                      const glm::vec4& color, const float maxWidth,
                      const float outlineWidthPx, const glm::vec4& outlineColor)
    {
        if (!font.Valid() || heightPx <= 0.f || utf8.empty())
            return;

        struct Placed
        {
            const FontGlyph* glyph = nullptr;
            float            x     = 0.f;
            float            y     = 0.f; ///< line top (logical px)
        };
        std::vector<Placed> placed;
        placed.reserve(utf8.size());

        const float lineHeight = heightPx;
        const float wrapW =
            maxWidth > 0.f ? maxWidth : (rect.w > 0.f ? rect.w : 0.f);

        float       penX = 0.f;
        float       penY = 0.f;
        std::size_t i    = 0;
        char32_t    cp   = 0;

        while (NextCodepoint(utf8, i, cp))
        {
            if (cp == U'\n')
            {
                penX = 0.f;
                penY += lineHeight;
                continue;
            }

            const FontGlyph* g = font.Find(cp);
            if (!g)
                g = font.Find(U'?');
            if (!g)
                continue;

            const float adv = g->advance * heightPx;
            if (wrapW > 0.f && penX > 0.f && penX + adv > wrapW && !IsSpace(cp))
            {
                penX = 0.f;
                penY += lineHeight;
            }

            if (IsSpace(cp) && wrapW > 0.f && penX + adv > wrapW)
            {
                penX = 0.f;
                penY += lineHeight;
                continue;
            }

            placed.push_back({ g, rect.x + penX, rect.y + penY });
            penX += adv;
        }

        if (placed.empty())
            return;

        const float pad = std::max(font.Padding(), 1.f);
        const float outlineSdf =
            std::clamp(outlineWidthPx / pad * 0.5f, 0.f, 0.49f);

        SpinLockGuard lock(mLock);
        for (const auto& p : placed)
        {
            const auto& g  = *p.glyph;
            const float gw = (g.planeRight - g.planeLeft) * heightPx;
            const float gh = (g.planeTop - g.planeBottom) * heightPx;
            if (gw <= 1e-5f || gh <= 1e-5f)
                continue;

            // rect.y is line-box top; baseline at +heightPx (em square).
            const float baselineY = p.y + heightPx;
            const float gx        = p.x + g.planeLeft * heightPx;
            const float gy        = baselineY - g.planeTop * heightPx;

            UiQuad q {};
            q.rect         = { gx, gy, gw, gh };
            q.uvRect       = g.uvRect;
            q.color        = color;
            q.textureIndex = font.HeapIndex();
            q.flags        = kUiFlagSdfGlyph;
            q.clipMax      = 1.f;
            q.outlineWidth = outlineSdf;
            q.outlineColor = outlineColor;
            pushUnlocked(q);
        }
    }

} // namespace FREYA_NAMESPACE
