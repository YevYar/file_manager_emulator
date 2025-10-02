#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <string_view>

#include "command_type.h"

/**
 * \brief A parser for user-input commands from an input stream.
 *
 * The CommandParser class is responsible for reading commands from an input stream,
 * interpreting them, and returning Command objects.
 * It supports commands such as `cp`, `md`, `mf`, `mv`, `rm`, as well
 * as error handling for invalid or malformed input.
 *
 * The parser works incrementally: it consumes commands from the provided input stream
 * and exposes them one by one through getNextCommand(). After reading all available
 * commands, hasMoreInput() returns false.
 */
class CommandParser final
{
    public:
        /**
         * \brief Constructs a CommandParser.
         *
         * \param inputStream The input stream to read commands from (for example, std::cin or std::ifstream).
         *        The parser does not take ownership of the stream.
         * \param inputStreamCleaner Optional callback executed on destruction
         *        to clean up or reset the input stream (for example, close the std::ifstream).
         */
        explicit CommandParser(std::istream& inputStream);

        /**
         * \brief Returns the string representation of the CommandName.
         */
        std::string commandNameToString(CommandName command) const;
        /**
         * \brief Reads and parses the next command from the input stream.
         *
         * \return A Command structure representing the parsed command.
         *         If parsing fails, the `error` field is set with a description.
         */
        Command getNextCommand();
        /**
         * \brief Checks if there is more input to be parsed.
         *
         * \return True if there is more data in the input stream, false otherwise.
         */
        bool    hasMoreInput();

    private:
        /**
         * \brief Converts a raw string into a CommandName.
         *
         * \param commandStr The raw command string (e.g., "cp", "md").
         * \return The corresponding CommandName enum value, or Unknown if invalid.
         */
        CommandName              parseCommandName(std::string_view commandStr) const;
        /**
         * \brief Parses arguments from a command string.
         *
         * Handles quoted strings and whitespace-separated tokens.
         *
         * \param commandStr The raw command string (excluding the command name).
         * \param outParsingError Output parameter for error messages, if any.
         * \return A vector of parsed arguments.
         */
        std::vector<std::string> parseCommandArguments(std::string_view            commandStr,
                                                       std::optional<std::string>& outParsingError) const;
        /**
         * \brief Helper: splits arguments by whitespace.
         *
         * \param parsedArguments Vector to append parsed arguments into.
         * \param commandStr The part of the command string, which contains arguments.
         */
        void                     parseCommandArgumentsByWhitespaces(std::vector<std::string>& parsedArguments,
                                                                    std::string_view          commandStr) const;

    private:
        std::istream& m_inStream;

};

#endif  // COMMAND_PARSER_H
