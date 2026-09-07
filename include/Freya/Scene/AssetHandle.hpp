#pragma once

#include "Freya/Config.hpp"

#include <cstdint>

namespace FREYA_NAMESPACE
{
    struct MeshTag
    {
    };
    struct MaterialTag
    {
    };
    struct TextureTag
    {
    };

    /**
     * @brief Typed opaque asset id (pool index). Default-constructed is null.
     */
    template <typename Tag>
    class AssetHandle
    {
      public:
        constexpr AssetHandle() = default;

        constexpr explicit AssetHandle(std::uint32_t id) : mId(id) {}

        [[nodiscard]] constexpr std::uint32_t Id() const { return mId; }

        [[nodiscard]] constexpr bool IsValid() const { return mId != 0; }

        constexpr explicit operator bool() const { return IsValid(); }

        constexpr auto operator<=>(const AssetHandle&) const = default;

      private:
        std::uint32_t mId = 0;
    };

    using MeshHandle     = AssetHandle<MeshTag>;
    using MaterialHandle = AssetHandle<MaterialTag>;
    using TextureHandle  = AssetHandle<TextureTag>;

} // namespace FREYA_NAMESPACE
