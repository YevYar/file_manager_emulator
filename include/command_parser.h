#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <functional>
#include <optional>
#include <string>
#include <vector>

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
         * \brief Enumeration of valid commands recognized by the CommandParser.
         */
        enum class CommandName
        {
            Cp, Md, Mf, Mv, Rm, Unknown
        };

        /**
         * \brief Parsed command structure.
         *
         * Represents a fully parsed command line, containing the command name,
         * its arguments, and optionally an error string if parsing failed.
         */
        struct Command final
        {
            CommandName                name = CommandName::Unknown;
            std::vector<std::string>   arguments;
            std::optional<std::string> error;
        };

    public:
        /**
         * \brief Constructs a CommandParser.
         *
         * \param inputStream The input stream to read commands from (for example, std::cin or std::ifstream).
         *        The parser does not take ownership of the stream.
         * \param inputStreamCleaner Optional callback executed on destruction
         *        to clean up or reset the input stream (for example, close the std::ifstream).
         */
        explicit CommandParser(std::istream& inputStream, std::function<void()> inputStreamCleaner = {});
        CommandParser(const CommandParser&) = delete;
        CommandParser(CommandParser&&)      = delete;

        ~CommandParser();

        CommandParser& operator=(const CommandParser&) = delete;
        CommandParser& operator=(CommandParser&&)      = delete;

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
        CommandName              parseCommandName(const std::string& commandStr) const;
        /**
         * \brief Parses arguments from a command string.
         *
         * Handles quoted strings and whitespace-separated tokens.
         *
         * \param commandStr The raw command string (excluding the command name).
         * \param outParsingError Output parameter for error messages, if any.
         * \return A vector of parsed arguments.
         */
        std::vector<std::string> parseCommandArguments(const std::string&          commandStr,
                                                       std::optional<std::string>& outParsingError) const;
        /**
         * \brief Helper: splits arguments by whitespace.
         *
         * \param parsedArguments Vector to append parsed arguments into.
         * \param commandStr The part of the command string, which contains arguments.
         * \param start Start index of the substring to parse.
         * \param length Length of the substring to parse.
         */
        void                     parseCommandArgumentsByWhitespaces(std::vector<std::string>& parsedArguments,
                                                                    const std::string& commandStr, std::size_t start,
                                                                    std::size_t length) const;
        /**
         * \brief Helper: trims leading and trailing whitespaces from a string.
         */
        void                     trim(std::string& str) const;

    private:
        std::istream&         m_inStream;
        std::function<void()> m_inStreamCleaner;
};

#endif  // COMMAND_PARSER_H
