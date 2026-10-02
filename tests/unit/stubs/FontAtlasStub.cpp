#include <Freya/Asset/FontAtlas.hpp>
#include <Freya/Asset/TexturePool.hpp>

// FreyaTests does not link TexturePool / Vulkan. Stub the FontAtlas symbols
// referenced by UiDraw::Text so UI unit tests stay device-free.
//
// Create() returns a fake-valid atlas with synthetic monospace glyphs so
// tests can count emitted UiFlags::SdfGlyph quads per rendered string
// (regression coverage for widget display text, e.g. ComboBox rows).

namespace FREYA_NAMESPACE
{
    FontAtlas FontAtlas::Create(TexturePool&, const std::string&, float,
                                std::uint32_t)
    {
        FontAtlas atlas;
        // Fake-valid handle: engaged flag only, never touches the GPU.
        atlas.mTexture = TextureHandle { 7 };
        FontGlyph glyph {};
        glyph.advance    = 0.5f;
        glyph.planeRight = 1.f;
        glyph.planeTop   = 1.f;
        for (char32_t cp = 32; cp < 127; ++cp)
            atlas.mGlyphs[cp] = glyph;
        return atlas;
    }

    std::uint32_t FontAtlas::HeapIndex() const
    {
        return TexturePool::BindlessIndex(mTexture);
    }

    const FontGlyph* FontAtlas::Find(char32_t codepoint) const
    {
        const auto it = mGlyphs.find(codepoint);
        return it != mGlyphs.end() ? &it->second : nullptr;
    }
} // namespace FREYA_NAMESPACE
