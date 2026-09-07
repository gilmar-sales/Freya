#pragma once

#include <Skirnir/Skirnir.hpp>

#include <cstdlib>
#include <string_view>

namespace FreyaExamples
{
    /**
     * @brief Console + file sinks for examples.
     *
     * Override the path with @c FREYA_LOG_FILE. Disable console with
     * @c FREYA_LOG_CONSOLE=0.
     */
    inline void ConfigureLogging(skr::LoggingExtension& logging,
                                 const char*            defaultLogFile)
    {
        const char* console = std::getenv("FREYA_LOG_CONSOLE");
        if (console == nullptr || std::string_view(console) != "0")
            logging.AddConsoleSink();

        const char* fromEnv = std::getenv("FREYA_LOG_FILE");
        logging.AddFileSink((fromEnv != nullptr && *fromEnv != '\0')
                                ? fromEnv
                                : defaultLogFile);
    }
} // namespace FreyaExamples
