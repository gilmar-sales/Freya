#pragma once

#include "Freya/Builders/FreyaOptionsBuilder.hpp"
#include "Freya/Config.hpp"

#include <Skirnir/Skirnir.hpp>

#include <functional>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Extension interface for Freya engine service registration.
     *
     * Implements skr::IExtension to register all Freya services
     * in the service provider. Use WithOptions() to configure
     * FreyaOptions before ConfigureServices() runs.
     */
    class FreyaExtension : public skr::IExtension
    {
      public:
        /**
         * @brief Fluent API to configure FreyaOptions.
         */
        FreyaExtension& WithOptions(
            const std::function<void(FreyaOptionsBuilder&)>&
                freyaOptionsBuilderFunc)
        {
            freyaOptionsBuilderFunc(mFreyaOptionsBuilder);
            return *this;
        }

      protected:
        void ConfigureServices(skr::ServiceCollection& services) override;

      private:
        FreyaOptionsBuilder mFreyaOptionsBuilder;
    };
} // namespace FREYA_NAMESPACE
