#include "Freya/Builders/FreyaOptionsBuilder.hpp"

namespace FREYA_NAMESPACE
{
    class FreyaExtension : public skr::IExtension
    {
      public:
        void ConfigureServices(Ref<skr::ServiceCollection> services) override;

        FreyaExtension& WithOptions(
            std::function<void(FreyaOptionsBuilder&)> freyaOptionsBuilderFunc)
        {
            freyaOptionsBuilderFunc(mFreyaOptionsBuilder);

            return *this;
        }

      private:
        FreyaOptionsBuilder mFreyaOptionsBuilder;
    };
} // namespace FREYA_NAMESPACE
