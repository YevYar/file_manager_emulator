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

        enum class NodeType
        {
            Directory,
            File,
            Invalid
        };

        struct PathInfo
        {
            std::string path;
            std::string basename;
            NodeType    type;
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
        bool md(std::string_view dirAbsolutePath);
        bool mf(std::string_view fileAbsolutePath);
        bool mv(std::string_view source, std::string_view destination);
        bool rm(std::string_view path);

    private:
        bool     executeCommand(const Command& command /*, std::string& outError*/);
        FsNode*  findNodeByPath(std::string_view nodePath, std::string& outError) const;
        bool     initCommandParser(std::string_view batchFilePath);
        FsNode*  getChildNode(const FsNode* node, const std::string& childName, std::string& outError) const;
        PathInfo getNodePathInfo(std::string_view nodeAbsolutePath, NodeType requiredNodeType = NodeType::Invalid) const;
        FsNode*  validateNodeCreation(NodeType requiredNodeType, const PathInfo& pathInfo, std::string_view nodePath,
                                      bool ignoreIfAlreadyExist) const;
        bool     validateNumberOfCommandArguments(const Command& command /*, std::string& outError*/) const;

    private:
        std::unique_ptr<Logger>        m_logger;
        std::ifstream                  m_fileInStream;
        std::unique_ptr<FsNode>        m_fsRoot;
        std::unique_ptr<CommandParser> m_parser;
};

#endif  // FILE_MANAGER_EMULATOR_H
