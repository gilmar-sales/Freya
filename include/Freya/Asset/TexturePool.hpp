#pragma once

#include "Freya/Scene/AssetHandle.hpp"

#include <Skirnir/Skirnir.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace FREYA_NAMESPACE
{
    class TexturePool
    {
      public:
        TexturePool(const skr::Arc<skr::ServiceProvider>& serviceProvider);

        ~TexturePool();

        /** @return texture handle, or nullopt when the file is
         * missing/unreadable.
         */
        std::optional<TextureHandle> CreateTextureFromFile(std::string path);

        TextureHandle CreateTextureFromMemory(const void*   pixels,
                                              std::uint32_t width,
                                              std::uint32_t height,
                                              std::uint32_t channels  = 4,
                                              std::uint32_t mipLevels = 0);

        [[nodiscard]] bool Contains(TextureHandle id) const;

        /**
         * @brief Bindless heap slot for @p id (white=0, black=1, textures at
         * id+2). Matches MaterialDescriptorResources::TextureHeapIndex.
         */
        [[nodiscard]] static std::uint32_t BindlessIndex(TextureHandle id)
        {
            return id.IsValid() ? id.Id() + 2u : 0u;
        }

        void Destroy(TextureHandle id);

      private:
        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };

} // namespace FREYA_NAMESPACE
