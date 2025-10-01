#ifndef FILE_MANAGER_EMULATOR_H
#define FILE_MANAGER_EMULATOR_H

#include <fstream>
#include <memory>
#include <unordered_map>

#include "command_type.h"

class CommandParser;
class Logger;

enum class ErrorCode
{
    NoError = 0,
    CannotOpenDataStream,
    CommandParsingError,
    CommandArgumentsError,
    LogicError
};

class FileManagerEmulator final
{
    public:
        struct FsNode final
        {
            std::string                                              name;
            bool                                                     isDirectory = true;
            std::unordered_map<std::string, std::unique_ptr<FsNode>> children;
        };

    public:
        FileManagerEmulator(std::unique_ptr<Logger> logger);
        FileManagerEmulator(const FileManagerEmulator&) = delete;
        FileManagerEmulator(FileManagerEmulator&&)      = delete;

        ~FileManagerEmulator();

        FileManagerEmulator& operator=(const FileManagerEmulator&) = delete;
        FileManagerEmulator& operator=(FileManagerEmulator&&)      = delete;

        void      printFileTree() const;
        ErrorCode run(std::string_view batchFilePath = "");

        bool cp(std::string_view source, std::string_view destination);
        // bool exec(const std::string& path);
        bool md(std::string_view path);
        bool mf(std::string_view pathWithFileName);
        bool mv(std::string_view source, std::string_view destination);
        bool rm(std::string_view path);

    private:
        bool    executeCommand(const Command& command /*, std::string& outError*/);
        FsNode* findNodeByPath(std::string_view nodePath, std::string& outError) const;
        bool    initCommandParser(std::string_view batchFilePath);
        bool    validateNumberOfCommandArguments(const Command& command /*, std::string& outError*/) const;

    private:
        std::unique_ptr<Logger>        m_logger;
        std::ifstream                  m_fileInStream;
        FsNode                         m_fsRoot;
        std::unique_ptr<CommandParser> m_parser;
        bool                           m_shouldPrintTreeOnDestruction = true;
};

#endif  // FILE_MANAGER_EMULATOR_H
