#include "Builders/ApplicationBuilder.hpp"

namespace FREYA_NAMESPACE
{
    ApplicationBuilder& ApplicationBuilder::WithWindow(std::function<void(WindowBuilder&)> windowBuilderFunc)
    {
        windowBuilderFunc(mWindowBuilder);

        return *this;
    }

    ApplicationBuilder& ApplicationBuilder::WithRenderer(std::function<void(RendererBuilder&)> rendererBuilderFunc)
    {
        rendererBuilderFunc(mRendererBuilder);

        return *this;
    }
} // namespace FREYA_NAMESPACE
