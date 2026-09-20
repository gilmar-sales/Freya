#include "Freya/Core/BillboardDraw.hpp"

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
    } // namespace

    void BillboardDraw::Text(
        const glm::vec3& worldPos, std::string_view utf8, const FontAtlas& font,
        const float heightMeters, const glm::vec4& color,
        const float outlineWidthPx, const glm::vec4& outlineColor,
        const BillboardAlign align, const BillboardLayer layer)
    {
        if (!font.Valid() || heightMeters <= 0.f || utf8.empty())
            return;

        struct Placed
        {
            const FontGlyph* glyph = nullptr;
            float            pen   = 0.f;
        };
        std::vector<Placed> placed;
        placed.reserve(utf8.size());

        float       width = 0.f;
        std::size_t i     = 0;
        char32_t    cp    = 0;
        while (NextCodepoint(utf8, i, cp))
        {
            const FontGlyph* g = font.Find(cp);
            if (!g)
                g = font.Find(U'?');
            if (!g)
                continue;
            placed.push_back({ g, width });
            width += g->advance * heightMeters;
        }
        if (placed.empty())
            return;

        const float   origin = -0.5f * width;
        SpinLockGuard lock(mLock);
        for (const auto& p : placed)
        {
            const auto& g  = *p.glyph;
            const float gw = (g.planeRight - g.planeLeft) * heightMeters;
            const float gh = (g.planeTop - g.planeBottom) * heightMeters;
            if (gw <= 1e-5f || gh <= 1e-5f)
                continue;

            Billboard b {};
            b.worldPos      = worldPos;
            b.size          = { gw, gh };
            b.color         = color;
            b.uvRect        = g.uvRect;
            b.textureIndex  = font.HeapIndex();
            b.align         = align;
            b.blend         = BillboardBlend::Alpha;
            b.layer         = layer;
            b.depthTest     = true;
            b.sdf           = true;
            b.clipMax       = 1.f;
            const float pad = std::max(font.Padding(), 1.f);
            b.outlineWidth =
                std::clamp(outlineWidthPx / pad * 0.5f, 0.f, 0.49f);
            b.outlineColor = outlineColor;
            b.localOffset  = {
                origin + p.pen +
                    0.5f * (g.planeLeft + g.planeRight) * heightMeters,
                0.5f * (g.planeBottom + g.planeTop) * heightMeters
            };
            pushUnlocked(b);
        }
    }

} // namespace FREYA_NAMESPACE
