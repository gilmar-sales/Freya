#include "Freya/Asset/FontAtlas.hpp"

#define STB_TRUETYPE_IMPLEMENTATION
#include "Freya/Vendor/stb_truetype.h"

#include "Freya/Asset/MaterialDescriptorResources.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

namespace FREYA_NAMESPACE
{
    namespace
    {
        constexpr std::uint32_t kAtlasSize = 1024;

        std::vector<unsigned char> ReadFile(const std::string& path)
        {
            std::ifstream in(path, std::ios::binary);
            if (!in)
                return {};
            in.seekg(0, std::ios::end);
            const auto size = static_cast<std::size_t>(in.tellg());
            in.seekg(0, std::ios::beg);
            std::vector<unsigned char> bytes(size);
            in.read(reinterpret_cast<char*>(bytes.data()),
                    static_cast<std::streamsize>(size));
            return bytes;
        }
    } // namespace

    std::uint32_t FontAtlas::HeapIndex() const
    {
        if (mTextureId == kInvalid)
            return 0;
        return MaterialDescriptorResources::TextureHeapIndex(mTextureId);
    }

    const FontGlyph* FontAtlas::Find(const char32_t codepoint) const
    {
        const auto it = mGlyphs.find(codepoint);
        if (it != mGlyphs.end())
            return &it->second;
        return nullptr;
    }

    FontAtlas FontAtlas::Create(TexturePool& textures, const std::string& path,
                                const float         pixelHeight,
                                const std::uint32_t padding)
    {
        FontAtlas atlas;
        atlas.mPixelHeight = pixelHeight;

        auto ttf = ReadFile(path);
        if (ttf.empty())
        {
            std::cerr << "FontAtlas: failed to read " << path << '\n';
            return atlas;
        }

        stbtt_fontinfo info {};
        if (!stbtt_InitFont(&info, ttf.data(), 0))
        {
            std::cerr << "FontAtlas: stbtt_InitFont failed for " << path
                      << '\n';
            return atlas;
        }

        const float scale = stbtt_ScaleForPixelHeight(&info, pixelHeight);
        const int   pad   = static_cast<int>(std::max(1u, padding));
        const unsigned char onEdge = 128;
        const float distScale =
            static_cast<float>(onEdge) / static_cast<float>(pad);

        std::vector<unsigned char> atlasPixels(kAtlasSize * kAtlasSize, 0);

        int packX = 1;
        int packY = 1;
        int rowH  = 0;

        auto tryPack = [&](int w, int h, int& outX, int& outY) -> bool {
            if (w <= 0 || h <= 0)
                return false;
            if (packX + w + 1 > static_cast<int>(kAtlasSize))
            {
                packX = 1;
                packY += rowH + 1;
                rowH = 0;
            }
            if (packY + h + 1 > static_cast<int>(kAtlasSize))
                return false;
            outX  = packX;
            outY  = packY;
            packX += w + 1;
            rowH = std::max(rowH, h);
            return true;
        };

        auto bakeCodepoint = [&](char32_t cp) {
            const int glyph = stbtt_FindGlyphIndex(&info, static_cast<int>(cp));
            if (glyph == 0 && cp != U' ')
                return;

            int advance = 0;
            int lsb     = 0;
            stbtt_GetGlyphHMetrics(&info, glyph, &advance, &lsb);

            FontGlyph g {};
            g.advance = (advance * scale) / pixelHeight;

            int w = 0, h = 0, xoff = 0, yoff = 0;
            unsigned char* sdf = stbtt_GetGlyphSDF(
                &info, scale, glyph, pad, onEdge, distScale, &w, &h, &xoff,
                &yoff);

            if (sdf && w > 0 && h > 0)
            {
                int px = 0, py = 0;
                if (!tryPack(w, h, px, py))
                {
                    stbtt_FreeSDF(sdf, nullptr);
                    std::cerr << "FontAtlas: atlas overflow\n";
                    return;
                }
                for (int row = 0; row < h; ++row)
                {
                    std::memcpy(
                        atlasPixels.data() +
                            (static_cast<std::size_t>(py + row) * kAtlasSize +
                             px),
                        sdf + static_cast<std::size_t>(row) * w, w);
                }
                stbtt_FreeSDF(sdf, nullptr);

                const float invA = 1.f / static_cast<float>(kAtlasSize);
                g.uvRect         = { static_cast<float>(px) * invA,
                                     static_cast<float>(py) * invA,
                                     static_cast<float>(px + w) * invA,
                                     static_cast<float>(py + h) * invA };
                g.planeLeft   = static_cast<float>(xoff) / pixelHeight;
                g.planeTop    = -static_cast<float>(yoff) / pixelHeight;
                g.planeRight  = static_cast<float>(xoff + w) / pixelHeight;
                g.planeBottom = -static_cast<float>(yoff + h) / pixelHeight;
            }

            atlas.mGlyphs[cp] = g;
        };

        for (char32_t cp = 32; cp <= 126; ++cp)
            bakeCodepoint(cp);
        for (char32_t cp = 160; cp <= 255; ++cp)
            bakeCodepoint(cp);

        atlas.mTextureId = textures.CreateTextureFromMemory(
            atlasPixels.data(), kAtlasSize, kAtlasSize, 1, 1);
        return atlas;
    }

} // namespace FREYA_NAMESPACE
