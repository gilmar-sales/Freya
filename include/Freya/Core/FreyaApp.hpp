#pragma once

/**
 * @file FreyaApp.hpp
 * @brief Application entry helpers that hide Skirnir builder boilerplate.
 */

#include "Freya/Builders/FreyaOptionsBuilder.hpp"
#include "Freya/Core/FreyaExtension.hpp"

#include <Skirnir/Skirnir.hpp>

#include <functional>

namespace FREYA_NAMESPACE
{
    /// Shared ownership alias for Freya objects (Skirnir Arc).
    template <typename T>
    using Ref = skr::Arc<T>;

    using ServiceProvider = skr::ServiceProvider;
    using ServiceScope    = skr::ServiceScope;

    /**
     * @brief Configure and run a Freya AbstractApplication subclass.
     *
     * @tparam AppT Subclass of AbstractApplication
     * @param configureOptions FreyaOptionsBuilder configuration (required)
     * @param configureLogging Optional LoggingExtension setup; when empty,
     *        a console sink is registered.
     */
    template <typename AppT>
    int RunApp(
        const std::function<void(FreyaOptionsBuilder&)>& configureOptions,
        const std::function<void(skr::LoggingExtension&)>&
            configureLogging = {})
    {
        auto builder = skr::ApplicationBuilder();
        builder.WithExtension<skr::LoggingExtension>(
            [configureLogging](skr::LoggingExtension& logging) {
                if (configureLogging)
                    configureLogging(logging);
                else
                    logging.AddConsoleSink();
            });
        builder.WithExtension<FreyaExtension>(
            [configureOptions](FreyaExtension freya) {
                if (configureOptions)
                    freya.WithOptions(configureOptions);
            });
        const auto app = builder.template Build<AppT>();
        app->Run();
        return 0;
    }

} // namespace FREYA_NAMESPACE
