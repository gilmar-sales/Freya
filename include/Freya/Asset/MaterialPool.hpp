#pragma once

#include "Freya/Asset/Material.hpp"
#include "Freya/Scene/AssetHandle.hpp"

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

        MaterialHandle CreateFromTextureFiles(
            std::vector<std::string> texturesPath);

        MaterialHandle Create(const MaterialCreateInfo& createInfo);

        void Update(MaterialHandle id, const MaterialCreateInfo& createInfo);

        [[nodiscard]] const MaterialCreateInfo& GetCreateInfo(
            MaterialHandle id) const;

        [[nodiscard]] bool Contains(MaterialHandle id) const;

        void Destroy(MaterialHandle id);

      private:
        std::unique_ptr<Impl> mImpl;
    };

} // namespace FREYA_NAMESPACE
