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
     *
     * Pool ids may be 0; validity is an engaged flag, not "id != 0".
     */
    template <typename Tag>
    class AssetHandle
    {
      public:
        constexpr AssetHandle() = default;

        constexpr explicit AssetHandle(std::uint32_t id) : mId(id), mValid(true)
        {
        }

        [[nodiscard]] constexpr std::uint32_t Id() const { return mId; }

        [[nodiscard]] constexpr bool IsValid() const { return mValid; }

        constexpr explicit operator bool() const { return IsValid(); }

        constexpr auto operator<=>(const AssetHandle& other) const
        {
            if (mValid != other.mValid)
                return mValid <=> other.mValid;
            return mId <=> other.mId;
        }

        constexpr bool operator==(const AssetHandle&) const = default;

      private:
        std::uint32_t mId    = 0;
        bool          mValid = false;
    };

    using MeshHandle     = AssetHandle<MeshTag>;
    using MaterialHandle = AssetHandle<MaterialTag>;
    using TextureHandle  = AssetHandle<TextureTag>;

} // namespace FREYA_NAMESPACE
