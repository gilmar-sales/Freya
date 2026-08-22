#include "Freya/Builders/PostProcessBuilder.hpp"

#include "Freya/Builders/BufferBuilder.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Internal/PostProcessImpl.hpp"

#include <memory>

namespace FREYA_NAMESPACE
{
    PostProcessBuilder::PostProcessBuilder(
        const skr::Arc<skr::ServiceProvider>& serviceProvider) :
        mServiceProvider(serviceProvider)
    {
    }

    skr::Arc<PostProcess> PostProcessBuilder::Build()
    {
        if (mFragmentRelative.empty())
            return {};

        auto impl              = std::make_unique<PostProcess::Impl>();
        impl->device           = mServiceProvider->GetService<Device>();
        impl->options          = mServiceProvider->GetService<FreyaOptions>();
        impl->serviceProvider  = mServiceProvider;
        impl->name             = mName;
        impl->fragmentRelative = mFragmentRelative;
        impl->vertexRelative   = mVertexRelative;
        impl->inputs           = mInputs;
        impl->pushConstantSize = mPushConstantSize;
        if (impl->inputs.empty())
            impl->inputs.push_back(PostProcessInput::SceneColor);
        if (impl->pushConstantSize > 0)
            impl->pushData.resize(impl->pushConstantSize);

        impl->maskBuffer =
            BufferBuilder(impl->device)
                .SetUsage(BufferUsage::Uniform)
                .SetSize(sizeof(PostProcessMaterialMask))
                .Build();

        auto effect = skr::MakeArc<PostProcess>(std::move(impl));
        return effect;
    }
} // namespace FREYA_NAMESPACE
