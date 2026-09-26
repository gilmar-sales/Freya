#include "Freya/Asset/SceneInstanceUpload.hpp"

namespace FREYA_NAMESPACE
{
    std::uint32_t MakeSceneInstanceFlags(bool castShadows, bool translucent,
                                         bool skinned)
    {
        std::uint32_t flags = 0;
        if (castShadows)
            flags |= kSceneInstanceFlagCastShadows;
        if (translucent)
            flags |= kSceneInstanceFlagTranslucent;
        if (skinned)
            flags |= kSceneInstanceFlagSkinned;
        return flags;
    }
} // namespace FREYA_NAMESPACE
