#pragma once

#include "Freya/Core/SpinLock.hpp"
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

        /**
         * @brief Bind an existing sampled image into the bindless heap.
         *
         * Does not own @p imageView / @p sampler (e.g. RenderTarget color).
         * Thread-safe vs other TexturePool mutations.
         *
         * @param imageView Opaque vk::ImageView / VkImageView
         * @param sampler   Opaque vk::Sampler / VkSampler
         */
        TextureHandle RegisterExternalImage(void*         imageView,
                                            void*         sampler,
                                            std::uint32_t width,
                                            std::uint32_t height);

        /**
         * @brief Remove an external registration (does not destroy GPU
         * objects).
         */
        void UnregisterExternal(TextureHandle id);

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
