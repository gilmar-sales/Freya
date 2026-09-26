#pragma once

#include "Freya/Core/PostProcess.hpp"

#include <Skirnir/Skirnir.hpp>

#include <string>
#include <vector>

namespace FREYA_NAMESPACE
{
    class PostProcessBuilder
    {
      public:
        PostProcessBuilder(
            const skr::Arc<skr::ServiceProvider>& serviceProvider);

        PostProcessBuilder& SetName(std::string name);

        PostProcessBuilder& SetFragment(std::string relativeSpv);

        PostProcessBuilder& SetVertex(std::string relativeSpv);

        PostProcessBuilder& SetInputs(std::vector<PostProcessInput> inputs);

        PostProcessBuilder& SetPushConstantSize(std::uint32_t size);

        skr::Arc<PostProcess> Build();

      private:
        skr::Arc<skr::ServiceProvider> mServiceProvider;
        std::string                    mName = "PostProcess";
        std::string                    mFragmentRelative;
        std::string mVertexRelative = "DeferredCompressed/composing.vert.spv";
        std::vector<PostProcessInput> mInputs { PostProcessInput::SceneColor };
        std::uint32_t                 mPushConstantSize = 0;
    };

} // namespace FREYA_NAMESPACE
