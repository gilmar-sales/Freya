#include "Freya/Builders/ApplicationBuilder.hpp"

namespace FREYA_NAMESPACE
{
    ApplicationBuilder& ApplicationBuilder::WithWindow(
        const std::function<void(WindowBuilder&)>& windowBuilderFunc)
    {
        mWindowBuilderFunc = windowBuilderFunc;

        return *this;
    }

    ApplicationBuilder& ApplicationBuilder::WithRenderer(
        const std::function<void(RendererBuilder&)>& rendererBuilderFunc)
    {
        mRendererBuilderFunc = rendererBuilderFunc;

        return *this;
    }
} // namespace FREYA_NAMESPACE
