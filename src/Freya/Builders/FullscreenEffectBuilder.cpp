#include "Freya/Builders/FullscreenEffectBuilder.hpp"

#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Internal/FullscreenEffectImpl.hpp"

#include <memory>

namespace FREYA_NAMESPACE
{
    FullscreenEffectBuilder::FullscreenEffectBuilder(
        const skr::Arc<skr::ServiceProvider>& serviceProvider) :
        mServiceProvider(serviceProvider)
    {
    }

    skr::Arc<FullscreenEffect> FullscreenEffectBuilder::Build()
    {
        if (mFragmentRelative.empty())
            return {};

        auto impl              = std::make_unique<FullscreenEffect::Impl>();
        impl->device           = mServiceProvider->GetService<Device>();
        impl->options          = mServiceProvider->GetService<FreyaOptions>();
        impl->serviceProvider  = mServiceProvider;
        impl->name             = mName;
        impl->fragmentRelative = mFragmentRelative;
        impl->vertexRelative   = mVertexRelative;
        impl->inputs           = mInputs;
        impl->pushConstantSize = mPushConstantSize;
        if (impl->inputs.empty())
            impl->inputs.push_back(EffectInput::SceneColor);
        if (impl->pushConstantSize > 0)
            impl->pushData.resize(impl->pushConstantSize);

        impl->maskBuffer =
            BufferBuilder(impl->device)
                .SetUsage(BufferUsage::Uniform)
                .SetSize(sizeof(FullscreenMaterialMask))
                .Build();

        auto effect = skr::MakeArc<FullscreenEffect>(std::move(impl));
        return effect;
    }
} // namespace FREYA_NAMESPACE
