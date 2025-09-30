#include "command_parser.h"

#include <algorithm>
#include <istream>
#include <limits>
#include <sstream>

namespace
{

int isSpace(unsigned char c)
{
    return std::isspace(c);
}

}  // namespace

CommandParser::CommandParser(std::istream& inputStream, std::function<void()> inputStreamCleaner) :
    m_inStream{inputStream}, m_inStreamCleaner{std::move(inputStreamCleaner)}
{
}

CommandParser::~CommandParser()
{
    if (m_inStreamCleaner)
    {
        m_inStreamCleaner();
    }
}

CommandParser::Command CommandParser::getNextCommand()
{
    if (!m_inStream)
    {
        return Command{};
    }

    auto cmdPartStr = std::string{};

    // Read command name
    m_inStream >> std::ws >> cmdPartStr;

    const auto cmdName = parseCommandName(cmdPartStr);
    if (cmdName == CommandName::Unknown)
    {
        const auto errorStr = std::string{"Unknown command is met: "} + cmdPartStr;
        m_inStream.ignore(std::numeric_limits<std::streamsize>::max(), '\n');  // Skip the rest of the command

        return Command{.name = CommandName::Unknown, .arguments = {}, .error = errorStr};
    }

    // Read command arguments
    std::getline(m_inStream, cmdPartStr);
    auto argumentsParsingError = std::optional<std::string>{};

    return Command{cmdName, parseCommandArguments(cmdPartStr, argumentsParsingError), argumentsParsingError};
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

CommandParser::CommandName CommandParser::parseCommandName(const std::string_view commandStr) const
{
    if (commandStr == "cp")
    {
        return CommandName::Cp;
    }
    else if (commandStr == "md")
    {
        return CommandName::Md;
    }
    else if (commandStr == "mf")
    {
        return CommandName::Mf;
    }
    else if (commandStr == "mv")
    {
        return CommandName::Mv;
    }
    else if (commandStr == "rm")
    {
        return CommandName::Rm;
    }

    return CommandName::Unknown;
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
        outParsingError = "Closing quotes \" is not found.";
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

void CommandParser::trim(std::string& str) const
{
    const auto first = std::find_if_not(str.begin(), str.end(), isSpace);
    const auto last  = std::find_if_not(str.rbegin(), str.rend(), isSpace).base();

    if (first < last)
    {
        str.assign(first, last);
    }
    else
    {
        str.clear();  // All spaces
    }
}
