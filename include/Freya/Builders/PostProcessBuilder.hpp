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

        PostProcessBuilder& SetName(std::string name)
        {
            mName = std::move(name);
            return *this;
        }

        PostProcessBuilder& SetFragment(std::string relativeSpv)
        {
            mFragmentRelative = std::move(relativeSpv);
            return *this;
        }

        PostProcessBuilder& SetVertex(std::string relativeSpv)
        {
            mVertexRelative = std::move(relativeSpv);
            return *this;
        }

        PostProcessBuilder& SetInputs(std::vector<PostProcessInput> inputs)
        {
            mInputs = std::move(inputs);
            return *this;
        }

        PostProcessBuilder& SetPushConstantSize(std::uint32_t size)
        {
            mPushConstantSize = size;
            return *this;
        }

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
