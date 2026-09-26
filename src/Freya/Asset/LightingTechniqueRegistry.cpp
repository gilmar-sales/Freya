#include "Freya/Asset/LightingTechniqueRegistry.hpp"

namespace FREYA_NAMESPACE
{
    void LightingTechniqueRegistry::SetFragment(std::string fragmentRelativeSpv)
    {
        mFragment = std::move(fragmentRelativeSpv);
    }

    void LightingTechniqueRegistry::Clear() { mFragment.clear(); }

    bool LightingTechniqueRegistry::HasOverride() const
    {
        return !mFragment.empty();
    }

    const std::string& LightingTechniqueRegistry::Fragment() const
    {
        return mFragment;
    }

    std::string LightingTechniqueRegistry::FragmentOrDefault() const
    {
        return HasOverride() ? mFragment : std::string(kDefaultFragment);
    }
} // namespace FREYA_NAMESPACE
