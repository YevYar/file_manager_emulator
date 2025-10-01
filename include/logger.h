#ifndef LOGGER_H
#define LOGGER_H

#include <string_view>

/**
 * \brief Base class for logging messages with different severity levels into the console.
 *
 * Provides methods to log errors, warnings, and informational messages.
 * Output formatting is consistent ("ERROR:", "WARNING:", "INFO:").
 * Derived classes can override writeLog() to customize log sinks (file, network, etc.).
 */
class Logger
{
    public:
        Logger() = default;

        virtual ~Logger() = default;

        /// Logs an error message (prepends "ERROR: " and optionally the command string).
        bool logError(std::string_view errorMessage, std::string_view commandString = "");

        /// Logs an informational message (prepends "INFO: " and optionally the command string).
        bool logInfo(std::string_view infoMessage, std::string_view commandString = "");

        /// Logs a warning message (prepends "WARNING: " and optionally the command string).
        bool logWarning(std::string_view warningMessage, std::string_view commandString = "");

    protected:
        /**
         * \brief Writes a formatted log string to the underlying sink.
         * 
         * \param log Formatted log message.
         * \return true if writing succeeded, false otherwise.
         */
        virtual bool writeLog(std::string_view log);
};

#endif  // LOGGER_H
