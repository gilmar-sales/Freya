#include <Freya/Asset/FontAtlas.hpp>
#include <Freya/Asset/TexturePool.hpp>

// FreyaTests does not link TexturePool / Vulkan. Stub the FontAtlas symbols
// referenced by UiDraw::Text so UI unit tests stay device-free.

namespace FREYA_NAMESPACE
{
    std::uint32_t FontAtlas::HeapIndex() const
    {
        return TexturePool::BindlessIndex(mTexture);
    }

    const FontGlyph* FontAtlas::Find(char32_t) const
    {
        return nullptr;
    }
} // namespace FREYA_NAMESPACE
