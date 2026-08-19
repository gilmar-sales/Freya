#pragma once

#include <Skirnir/Skirnir.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace FREYA_NAMESPACE
{
    class TexturePool
    {
      public:
        TexturePool(const skr::Arc<skr::ServiceProvider>& serviceProvider);

        ~TexturePool();

        std::uint32_t CreateTextureFromFile(std::string path);

        std::uint32_t CreateTextureFromMemory(const void*   pixels,
                                              std::uint32_t width,
                                              std::uint32_t height,
                                              std::uint32_t channels  = 4,
                                              std::uint32_t mipLevels = 0);

      private:
        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };

} // namespace FREYA_NAMESPACE
