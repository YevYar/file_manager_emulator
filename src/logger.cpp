#include "logger.h"

#include <iostream>
#include <string>

namespace
{
std::string makeLogString(const std::string_view logType, const std::string_view commandString,
                          std::string_view errorMessage)
{
    auto log = std::string{};

    if (commandString.empty())
    {
        log.reserve(logType.size() + errorMessage.size());
        log.append(logType).append(errorMessage);
    }
    else
    {
        log.reserve(logType.size() + commandString.size() + 3 + errorMessage.size());
        log.append(logType).append("[").append(commandString).append("] ").append(errorMessage);
    }

    return log;
}
}  // namespace

bool Logger::logError(const std::string_view errorMessage, const std::string_view commandString)
{
    return writeLog(makeLogString("ERROR: ", commandString, errorMessage));
}

bool Logger::logInfo(const std::string_view infoMessage, const std::string_view commandString)
{
    return writeLog(makeLogString("INFO: ", commandString, infoMessage));
}

bool Logger::logWarning(const std::string_view warningMessage, const std::string_view commandString)
{
    return writeLog(makeLogString("WARNING: ", commandString, warningMessage));
}

bool Logger::writeLog(const std::string_view log)
{
    std::cout << log << std::endl;
    return static_cast<bool>(std::cout);
}
