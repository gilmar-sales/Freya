#include "Builders/ApplicationBuilder.hpp"

namespace FREYA_NAMESPACE
{
    ApplicationBuilder& ApplicationBuilder::WithWindow(std::function<void(WindowBuilder&)> windowBuilderFunc)
    {
        windowBuilderFunc(mWindowBuilder);

        return *this;
    }
} // namespace FREYA_NAMESPACE
