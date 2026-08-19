#pragma once

#include "Freya/Core/FullscreenEffect.hpp"

#include <Skirnir/Skirnir.hpp>

#include <string>
#include <vector>

namespace FREYA_NAMESPACE
{
    class FullscreenEffectBuilder
    {
      public:
        FullscreenEffectBuilder(
            const skr::Arc<skr::ServiceProvider>& serviceProvider);

        FullscreenEffectBuilder& SetName(std::string name)
        {
            mName = std::move(name);
            return *this;
        }

        FullscreenEffectBuilder& SetFragment(std::string relativeSpv)
        {
            mFragmentRelative = std::move(relativeSpv);
            return *this;
        }

        FullscreenEffectBuilder& SetVertex(std::string relativeSpv)
        {
            mVertexRelative = std::move(relativeSpv);
            return *this;
        }

        FullscreenEffectBuilder& SetInputs(std::vector<EffectInput> inputs)
        {
            mInputs = std::move(inputs);
            return *this;
        }

        FullscreenEffectBuilder& SetPushConstantSize(std::uint32_t size)
        {
            mPushConstantSize = size;
            return *this;
        }

        skr::Arc<FullscreenEffect> Build();

      private:
        skr::Arc<skr::ServiceProvider> mServiceProvider;
        std::string                    mName = "FullscreenEffect";
        std::string                    mFragmentRelative;
        std::string mVertexRelative = "DeferredCompressed/composing.vert.spv";
        std::vector<EffectInput> mInputs { EffectInput::SceneColor };
        std::uint32_t            mPushConstantSize = 0;
    };

} // namespace FREYA_NAMESPACE
