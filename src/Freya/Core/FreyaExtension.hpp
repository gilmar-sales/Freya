#include "Freya/Builders/FreyaOptionsBuilder.hpp"

namespace FREYA_NAMESPACE
{
    /**
     * @brief Extension interface for Freya engine service registration.
     *
     * Implements skr::IExtension to register all Freya services
     * in the service provider. Use WithOptions() to configure
     * FreyaOptions before ConfigureServices() is called.
     */
    class FreyaExtension : public skr::IExtension
    {
      public:
        /**
         * @brief Configures services in the service collection.
         * Registers all builders as transient, all core objects as singletons.
         * @param services Service collection to configure
         */
        void ConfigureServices(Ref<skr::ServiceCollection> services) override;

        /**
         * @brief Fluent API to configure FreyaOptions.
         * @param freyaOptionsBuilderFunc Lambda receiving FreyaOptionsBuilder
         * @return Reference to this for chaining
         */
        FreyaExtension& WithOptions(
            std::function<void(FreyaOptionsBuilder&)> freyaOptionsBuilderFunc)
        {
            freyaOptionsBuilderFunc(mFreyaOptionsBuilder);

            return *this;
        }

      private:
        FreyaOptionsBuilder
            mFreyaOptionsBuilder; ///< Builder for FreyaOptions configuration
    };
} // namespace FREYA_NAMESPACE
