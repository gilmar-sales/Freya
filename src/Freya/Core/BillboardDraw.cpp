#include "Freya/Core/BillboardDraw.hpp"

#include "Freya/Asset/FontAtlas.hpp"

#include <algorithm>
#include <string_view>
#include <vector>

namespace FREYA_NAMESPACE
{
    BillboardDraw::BillboardDraw(const std::uint32_t maxQuads) :
        mMaxQuads(std::max(1u, maxQuads))
    {
        mQuads.reserve(std::min(mMaxQuads, 256u));
    }

    void BillboardDraw::Clear()
    {
        mQuads.clear();
    }

    void BillboardDraw::Quad(const Billboard& billboard)
    {
        if (mQuads.size() >= mMaxQuads)
            return;
        mQuads.push_back(billboard);
    }

    void BillboardDraw::HealthBar(const glm::vec3& headPos, const float width,
                                  const float height, const float fill01,
                                  const glm::vec4& bg, const glm::vec4& fg)
    {
        const float fill = std::clamp(fill01, 0.f, 1.f);
        Billboard   plate {};
        plate.worldPos     = headPos;
        plate.size         = { width, height };
        plate.align        = BillboardAlign::Cylindrical;
        plate.blend        = BillboardBlend::Alpha;
        plate.layer        = BillboardLayer::Ui;
        plate.depthTest    = true;
        plate.textureIndex = 0;

        plate.color   = bg;
        plate.clipMax = 1.f;
        Quad(plate);

        plate.color   = fg;
        plate.clipMax = fill;
        Quad(plate);
    }

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

    void BillboardDraw::Text(const glm::vec3& worldPos, std::string_view utf8,
                             const FontAtlas& font, const float heightMeters,
                             const glm::vec4& color, const BillboardAlign align,
                             const BillboardLayer layer)
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

        const float origin = -0.5f * width;
        for (const auto& p : placed)
        {
            const auto& g  = *p.glyph;
            const float gw = (g.planeRight - g.planeLeft) * heightMeters;
            const float gh = (g.planeTop - g.planeBottom) * heightMeters;
            if (gw <= 1e-5f || gh <= 1e-5f)
                continue;

            Billboard b {};
            b.worldPos     = worldPos;
            b.size         = { gw, gh };
            b.color        = color;
            b.uvRect       = g.uvRect;
            b.textureIndex = font.HeapIndex();
            b.align        = align;
            b.blend        = BillboardBlend::Alpha;
            b.layer        = layer;
            b.depthTest    = true;
            b.sdf          = true;
            b.clipMax      = 1.f;
            b.localOffset  = {
                origin + p.pen +
                    0.5f * (g.planeLeft + g.planeRight) * heightMeters,
                0.5f * (g.planeBottom + g.planeTop) * heightMeters
            };
            Quad(b);
        }
    }

} // namespace FREYA_NAMESPACE
