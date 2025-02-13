#include "Freya/Builders/ApplicationBuilder.hpp"

namespace FREYA_NAMESPACE
{
    ApplicationBuilder& ApplicationBuilder::WithWindow(const std::function<void(WindowBuilder&)>& windowBuilderFunc)
    {
        windowBuilderFunc(mWindowBuilder);

        return *this;
    }

    ApplicationBuilder& ApplicationBuilder::WithRenderer(const std::function<void(RendererBuilder&)>& rendererBuilderFunc)
    {
        rendererBuilderFunc(mRendererBuilder);

        return *this;
    }
} // namespace FREYA_NAMESPACE
