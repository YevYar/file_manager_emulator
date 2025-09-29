#include "command_parser.h"

#include <algorithm>
#include <istream>
#include <limits>
#include <sstream>

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

CommandParser::CommandName CommandParser::parseCommandName(const std::string& commandStr) const
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

std::vector<std::string> CommandParser::parseCommandArguments(const std::string&          commandStr,
                                                              std::optional<std::string>& outParsingError) const
{
    constexpr auto quotesChar = '"';
    auto           startPos = std::size_t{0}, quotesPos = std::size_t{0};
    auto           quotesCounter   = 0;
    auto           parsedArguments = std::vector<std::string>{};

    for (quotesPos = commandStr.find_first_of(quotesChar, startPos); quotesPos != std::string::npos;
         quotesPos = commandStr.find_first_of(quotesChar, startPos))
    {
        if (quotesCounter % 2 == 0)
        {
            parseCommandArgumentsByWhitespaces(parsedArguments, commandStr, startPos, quotesPos - startPos);
        }
        else
        {
            // The part between open " and end " is one argument
            auto arg = commandStr.substr(startPos, quotesPos - startPos);
            trim(arg);
            if (!arg.empty())
            {
                parsedArguments.push_back(std::move(arg));
            }
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
    parseCommandArgumentsByWhitespaces(parsedArguments, commandStr, startPos, commandStr.length() - startPos);
    return parsedArguments;
}

void CommandParser::parseCommandArgumentsByWhitespaces(std::vector<std::string>& parsedArguments,
                                                       const std::string& commandStr, std::size_t start,
                                                       std::size_t length) const
{
    if (length == 0)
    {
        return;
    }

    const auto cmdPart = commandStr.substr(start, length);
    auto       iss     = std::istringstream{cmdPart};

    auto arg = std::string{};
    while (iss >> arg)
    {
        parsedArguments.push_back(std::move(arg));
    }
}

void CommandParser::trim(std::string& str) const
{
    const auto isSpace = [](unsigned char c)
    {
        return std::isspace(c);
    };

    auto first = std::find_if_not(str.begin(), str.end(), isSpace);
    auto last  = std::find_if_not(str.rbegin(), str.rend(), isSpace).base();

    if (first < last)
    {
        str.assign(first, last);
    }
    else
    {
        str.clear();  // All spaces
    }
}
