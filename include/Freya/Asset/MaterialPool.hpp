#pragma once

#include "Freya/Asset/Material.hpp"

#include <Skirnir/Skirnir.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace FREYA_NAMESPACE
{
    class TexturePool;

    class MaterialPool
    {
      public:
        struct Impl;

        MaterialPool(const skr::Arc<skr::ServiceProvider>& serviceProvider);

        ~MaterialPool();

        std::uint32_t CreateFromTextureFiles(
            std::vector<std::string> texturesPath);

        std::uint32_t Create(const MaterialCreateInfo& createInfo);

        void Update(std::uint32_t id, const MaterialCreateInfo& createInfo);

        [[nodiscard]] const MaterialCreateInfo& GetCreateInfo(
            std::uint32_t id) const;

        [[nodiscard]] bool Contains(std::uint32_t id) const;

        void Destroy(std::uint32_t id);

      private:
        std::unique_ptr<Impl> mImpl;
    };

} // namespace FREYA_NAMESPACE
