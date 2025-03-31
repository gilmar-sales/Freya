#include "ApplicationBuilder.hpp"

namespace FREYA_NAMESPACE
{
    ApplicationBuilder& ApplicationBuilder::WithOptions(
        std::function<void(FreyaOptions&)> freyaOptionsBuilderFunc)
    {
        mFreyaOptionsFunc = freyaOptionsBuilderFunc;

        return *this;
    }

} // namespace FREYA_NAMESPACE
