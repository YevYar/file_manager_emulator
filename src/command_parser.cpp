#include "command_parser.h"

#include <algorithm>
#include <istream>
#include <limits>
#include <map>

#include "helpers.h"

namespace
{
const std::map<std::string, CommandName> commandsMap = {
  {"cp", CommandName::Cp},
  {"md", CommandName::Md},
  {"mf", CommandName::Mf},
  {"mv", CommandName::Mv},
  {"rm", CommandName::Rm}
};

}  // namespace

CommandParser::CommandParser(std::istream& inputStream) : m_inStream{inputStream}
{
}

std::string CommandParser::commandNameToString(CommandName command) const
{
    for (const auto& [str, cmd] : commandsMap)
    {
        if (cmd == command)
        {
            return str;
        }
    }
    return "unknown";
}

Command CommandParser::getNextCommand()
{
    if (!m_inStream)
    {
        return Command{};
    }

    auto cmdPartStr = std::string{};

    // Read command name
    m_inStream >> std::ws >> cmdPartStr;

    auto       commandString = cmdPartStr;
    const auto cmdName       = parseCommandName(cmdPartStr);
    if (cmdName == CommandName::Unknown)
    {
        const auto errorStr = std::string{"Unknown command is met: "} + cmdPartStr;
        m_inStream.ignore(std::numeric_limits<std::streamsize>::max(), '\n');  // Skip the rest of the command

        return Command{.commandString = "", .name = CommandName::Unknown, .arguments = {}, .error = errorStr};
    }

    // Read command arguments
    std::getline(m_inStream, cmdPartStr);
    commandString              += cmdPartStr;
    auto argumentsParsingError  = std::optional<std::string>{};

    return Command{.commandString = commandString,
                   .name          = cmdName,
                   .arguments     = parseCommandArguments(cmdPartStr, argumentsParsingError),
                   .error         = argumentsParsingError};
}

bool CommandParser::hasMoreInput()
{
    m_inStream >> std::ws;

    if (!m_inStream.good())
    {
        return false;
    }

    const auto c = m_inStream.peek();
    return c != EOF;
}

CommandName CommandParser::parseCommandName(const std::string_view commandStr) const
{
    const auto cmd = std::string{commandStr};
    return commandsMap.contains(cmd) ? commandsMap.at(cmd) : CommandName::Unknown;
}

std::vector<std::string> CommandParser::parseCommandArguments(const std::string_view      commandStr,
                                                              std::optional<std::string>& outParsingError) const
{
    constexpr auto quotesChar = '"';
    auto           startPos = std::size_t{0}, quotesPos = std::size_t{0};
    auto           quotesCounter   = 0;
    auto           parsedArguments = std::vector<std::string>{};

    const auto findNextQuotes = [=, &startPos]()
    {
        return commandStr.find_first_of(quotesChar, startPos);
    };

    for (quotesPos = findNextQuotes(); quotesPos != std::string::npos; quotesPos = findNextQuotes())
    {
        if (quotesCounter % 2 == 0)
        {
            parseCommandArgumentsByWhitespaces(parsedArguments, commandStr.substr(startPos, quotesPos - startPos));
        }
        else
        {
            // The part between open " and end " is one argument
            auto arg = std::string{commandStr.substr(startPos, quotesPos - startPos)};
            trim(arg);

            if (arg.empty())
            {
                outParsingError = "Empty argument \"\" is found.";
                return parsedArguments;
            }

            parsedArguments.push_back(std::move(arg));
        }

        startPos = quotesPos + 1;
        ++quotesCounter;
    }

    if (quotesCounter % 2 != 0)
    {
        outParsingError = "Closing quotes \" symbol is not found.";
        return parsedArguments;
    }

    // Parse the rest of the string
    parseCommandArgumentsByWhitespaces(parsedArguments, commandStr.substr(startPos, commandStr.length() - startPos));
    return parsedArguments;
}

void CommandParser::parseCommandArgumentsByWhitespaces(std::vector<std::string>& parsedArguments,
                                                       const std::string_view    commandStr) const
{
    if (commandStr.empty())
    {
        return;
    }

    auto startPos = std::size_t{0}, endPos = std::size_t{0};

    const auto findNextWord = [=, &startPos]()
    {
        return std::find_if_not(commandStr.begin() + startPos, commandStr.end(), isSpace) - commandStr.begin();
    };

    for (startPos = findNextWord(); startPos < commandStr.size(); startPos = findNextWord())
    {
        endPos = std::find_if(commandStr.begin() + startPos, commandStr.end(), isSpace) - commandStr.begin();

        if (startPos - endPos > 0)
        {
            parsedArguments.emplace_back(commandStr.substr(startPos, endPos - startPos));
        }

        startPos = endPos;
    }
}
