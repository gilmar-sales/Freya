#pragma once

#include "Freya/Asset/FontAtlas.hpp"
#include "Freya/Core/UiDraw.hpp"
#include "Freya/Core/UiTypes.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Game UI theme tokens (ImGui-style).
     */
    struct UiStyle
    {
        std::array<glm::vec4, static_cast<std::size_t>(UiCol::COUNT)> colors {};
        std::array<float, static_cast<std::size_t>(UiVar::COUNT)>     vars {};

        const FontAtlas* font = nullptr;

        TextureHandle panelSlice {};
        TextureHandle buttonSlice {};
        glm::vec4     sliceMargins { 8.f, 8.f, 8.f, 8.f };

        [[nodiscard]] static UiStyle Default();

        [[nodiscard]] glm::vec4& Color(UiCol c)
        {
            return colors[static_cast<std::size_t>(c)];
        }
        [[nodiscard]] const glm::vec4& Color(UiCol c) const
        {
            return colors[static_cast<std::size_t>(c)];
        }
        [[nodiscard]] float& Var(UiVar v)
        {
            return vars[static_cast<std::size_t>(v)];
        }
        [[nodiscard]] float Var(UiVar v) const
        {
            return vars[static_cast<std::size_t>(v)];
        }
    };

} // namespace FREYA_NAMESPACE
