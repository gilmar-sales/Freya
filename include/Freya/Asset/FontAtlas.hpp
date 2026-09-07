#pragma once

#include "Freya/Asset/TexturePool.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    struct FontGlyph
    {
        glm::vec4 uvRect { 0.f, 0.f, 1.f, 1.f };
        float     planeLeft   = 0.f;
        float     planeBottom = 0.f;
        float     planeRight  = 0.f;
        float     planeTop    = 0.f;
        float     advance     = 0.f;
    };

    /**
     * @brief CPU-baked SDF atlas for Latin-1 nameplates (TMP/Slate-style).
     */
    class FontAtlas
    {
      public:
        static FontAtlas Create(TexturePool& textures, const std::string& path,
                                float         pixelHeight = 48.f,
                                std::uint32_t padding     = 8);

        [[nodiscard]] bool Valid() const { return mTexture.IsValid(); }

        [[nodiscard]] TextureHandle TextureId() const { return mTexture; }

        [[nodiscard]] std::uint32_t HeapIndex() const;

        [[nodiscard]] const FontGlyph* Find(char32_t codepoint) const;

        [[nodiscard]] float PixelHeight() const { return mPixelHeight; }

        [[nodiscard]] float Padding() const { return mPadding; }

      private:
        TextureHandle                           mTexture {};
        float                                   mPixelHeight = 48.f;
        float                                   mPadding     = 8.f;
        std::unordered_map<char32_t, FontGlyph> mGlyphs;
    };

} // namespace FREYA_NAMESPACE
