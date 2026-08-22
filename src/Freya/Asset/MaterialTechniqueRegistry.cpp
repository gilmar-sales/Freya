#include "Freya/Asset/MaterialTechniqueRegistry.hpp"

namespace FREYA_NAMESPACE
{
    std::uint32_t MaterialTechniqueRegistry::Register(
        std::string name, std::string fragmentRelativeSpv)
    {
        if (fragmentRelativeSpv.empty())
            return kDefaultTechnique;
        if (mEntries.size() + 1u >= kMaxTechniques)
            return kDefaultTechnique;

        mEntries.push_back(Entry {
            .name             = std::move(name),
            .fragmentRelative = std::move(fragmentRelativeSpv),
        });
        return static_cast<std::uint32_t>(mEntries.size());
    }
} // namespace FREYA_NAMESPACE
