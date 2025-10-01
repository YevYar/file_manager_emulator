#ifndef COMMAND_TYPE_H
#define COMMAND_TYPE_H

#include <optional>
#include <string>
#include <vector>

/**
 * \brief Enumeration of valid commands recognized by the File Manager Emulator.
 */
enum class CommandName
{
    Cp,
    Md,
    Mf,
    Mv,
    Rm,
    Unknown
};

/**
 * \brief Parsed command structure.
 *
 * Represents a fully parsed command line, containing the command name,
 * its arguments, and optionally an error string if parsing failed.
 */
struct Command final
{
        std::string                commandString;
        CommandName                name = CommandName::Unknown;
        std::vector<std::string>   arguments;
        std::optional<std::string> error;
};

#endif  // COMMAND_TYPE_H
