#pragma once

#include "Freya/Config.hpp"

#include <string>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Global override for the deferred lighting fragment shader.
     *
     * Default is `DeferredCompressed/lighting.frag.spv`. Call SetFragment
     * with a SPIR-V path relative to FreyaOptions::shaderRoot, then
     * Renderer::RebuildSwapChain. The override must match the stock
     * lighting descriptor layout (bindings 0–15), push constant
     * `debugMode`, fullscreen vertex (`lighting.vert`), and additive HDR
     * output.
     */
    class LightingTechniqueRegistry
    {
      public:
        static constexpr const char* kDefaultFragment =
            "DeferredCompressed/lighting.frag.spv";

        /**
         * @brief Select a custom lighting fragment (empty clears override).
         */
        void SetFragment(std::string fragmentRelativeSpv)
        {
            mFragment = std::move(fragmentRelativeSpv);
        }

        void Clear() { mFragment.clear(); }

        [[nodiscard]] bool HasOverride() const { return !mFragment.empty(); }

        /**
         * @brief Active relative path, or empty when using the stock default.
         */
        [[nodiscard]] const std::string& Fragment() const { return mFragment; }

        /**
         * @brief Path loaded by DeferredCompressedPassBuilder.
         */
        [[nodiscard]] std::string FragmentOrDefault() const
        {
            return HasOverride() ? mFragment : std::string(kDefaultFragment);
        }

      private:
        std::string mFragment;
    };

} // namespace FREYA_NAMESPACE
