#include "Freya/Builders/FullscreenEffectBuilder.hpp"

namespace FREYA_NAMESPACE
{
    skr::Arc<FullscreenEffect> FullscreenEffectBuilder::Build()
    {
        if (mFragmentRelative.empty())
            return {};

        auto inputs = mInputs;
        if (inputs.empty())
            inputs.push_back(EffectInput::SceneColor);

        return skr::MakeArc<FullscreenEffect>(
            mDevice,
            mFreyaOptions,
            mServiceProvider,
            mName,
            mFragmentRelative,
            mVertexRelative,
            std::move(inputs),
            mPushConstantSize);
    }
} // namespace FREYA_NAMESPACE
