#pragma once

#include "Freya/Config.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Registry of custom opaque G-buffer fragment techniques.
     *
     * Technique 0 is always the stock deferred PBR G-buffer. Apps register
     * additional fragments (same vertex contract / attachments as
     * `DeferredCompressed/gbuffer.frag`) and assign
     * `MaterialCreateInfo::techniqueId`. SPIR-V paths are relative to
     * `FreyaOptions::shaderRoot`. Register during `StartUp` before the first
     * deferred rebuild.
     */
    class MaterialTechniqueRegistry
    {
      public:
        static constexpr std::uint32_t kDefaultTechnique = 0;
        static constexpr std::uint32_t kMaxTechniques    = 8;

        struct Entry
        {
            std::string name;
            std::string fragmentRelative;
        };

        /**
         * @brief Register a custom G-buffer fragment.
         * @return Technique id in [1, kMaxTechniques), or 0 on failure.
         */
        std::uint32_t Register(std::string name,
                               std::string fragmentRelativeSpv);

        [[nodiscard]] std::uint32_t Count() const
        {
            return static_cast<std::uint32_t>(mEntries.size()) + 1u;
        }

        [[nodiscard]] const Entry* Get(std::uint32_t techniqueId) const
        {
            if (techniqueId == kDefaultTechnique ||
                techniqueId > mEntries.size())
                return nullptr;
            return &mEntries[techniqueId - 1u];
        }

        [[nodiscard]] const std::vector<Entry>& Entries() const
        {
            return mEntries;
        }

      private:
        std::vector<Entry> mEntries;
    };

} // namespace FREYA_NAMESPACE
