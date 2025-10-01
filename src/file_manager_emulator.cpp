#include "file_manager_emulator.h"

#include <cstring>
#include <format>
#include <fstream>
#include <iostream>
#include <map>

#include "command_parser.h"
#include "logger.h"

FileManagerEmulator::FileManagerEmulator(std::unique_ptr<Logger> logger) :
    m_logger{logger ? std::move(logger) : std::make_unique<Logger>()}, m_fsRoot{"/", true, {}}
{
}

FileManagerEmulator::~FileManagerEmulator()
{
    if (m_shouldPrintTreeOnDestruction)
    {
        printFileTree();
    }

    if (m_fileInStream.is_open())
    {
        m_fileInStream.close();
    }
}

void FileManagerEmulator::printFileTree() const
{
    // m_logger.info The FileManagerEmulator is finished with/without error. Result file tree:
}

ErrorCode FileManagerEmulator::run(const std::string_view batchFilePath)
{
    if (!initCommandParser(batchFilePath))
    {
        return ErrorCode::CannotOpenDataStream;
    }

    while (m_parser->hasMoreInput())
    {
        const auto command = m_parser->getNextCommand();

        if (command.name == CommandName::Unknown)
        {
            m_logger->logError(command.error.has_value() ? command.error.value() : "Uknown command is met.");
            return ErrorCode::CommandParsingError;
        }
        else if (command.error.has_value())
        {
            m_logger->logError(command.error.value(), command.commandString);
            return ErrorCode::CommandParsingError;
        }

        // auto currentError = std::string{};

        if (!validateNumberOfCommandArguments(command /*, currentError*/))
        {
            // m_logger. error
            return ErrorCode::CommandArgumentsError;
        }

        if (!executeCommand(command /*, currentError*/))
        {
            // m_logger. error
            return ErrorCode::LogicError;
        }
    }

    printFileTree();
    m_shouldPrintTreeOnDestruction = false;
    return ErrorCode::NoError;
}

bool FileManagerEmulator::cp(std::string_view source, std::string_view destination)
{
    return false;
}

bool FileManagerEmulator::md(std::string_view path)
{
    return false;
}

bool FileManagerEmulator::mf(std::string_view pathWithFileName)
{
    return false;
}

bool FileManagerEmulator::mv(std::string_view source, std::string_view destination)
{
    return false;
}

bool FileManagerEmulator::rm(std::string_view path)
{
    return false;
}

bool FileManagerEmulator::executeCommand(const Command& command /*, std::string& outError*/)
{
    switch (command.name)
    {
        case CommandName::Cp:
            return cp(command.arguments[0], command.arguments[1]);
        case CommandName::Md:
            return md(command.arguments[0]);
        case CommandName::Mf:
            return mf(command.arguments[0]);
        case CommandName::Mv:
            return mv(command.arguments[0], command.arguments[1]);
        case CommandName::Rm:
            return rm(command.arguments[0]);
        default:
            return false;
    }
}

FileManagerEmulator::FsNode* FileManagerEmulator::findNodeByPath(std::string_view nodePath, std::string& outError) const
{
    // only / is acceptible as path separator
    return nullptr;
}

bool FileManagerEmulator::initCommandParser(const std::string_view batchFilePath)
{
    if (!batchFilePath.empty())
    {
        m_fileInStream = std::ifstream(std::string{batchFilePath}, std::ifstream::in);

        if (m_fileInStream.is_open())
        {
            m_parser = std::make_unique<CommandParser>(m_fileInStream);
        }
        else
        {
            const auto errorMsg =
              std::format("{}: {}. {}", batchFilePath, "Cannot open the batch file for reading", std::strerror(errno));
            m_logger->logError(errorMsg);
            return false;
        }
    }
    else
    {
        m_parser = std::make_unique<CommandParser>(std::cin);
    }

    return m_parser != nullptr;
}

bool FileManagerEmulator::validateNumberOfCommandArguments(const Command& command /*, std::string& outError*/) const
{
    static const auto commandsArgumentNum = std::map<CommandName, std::size_t>{
      {CommandName::Cp, 2},
      {CommandName::Md, 1},
      {CommandName::Mf, 1},
      {CommandName::Mv, 2},
      {CommandName::Rm, 1}
    };

    const auto numArgsToAccept = commandsArgumentNum.at(command.name);
    const auto numPassedArgs   = command.arguments.size();

    if (numPassedArgs != numArgsToAccept)
    {
        // outError = std::format("The command {commandName} accepts {argsNumRequired} (was passed {argsNumReal})")
        m_logger->logError(std::format("Command {} accepts {} argument(-s) (the number of passed arguments is {}).",
                                       m_parser->commandNameToString(command.name), numArgsToAccept, numPassedArgs),
                           command.commandString);
        return false;
    }

    return true;
}
