#ifndef FILE_MANAGER_EMULATOR_H
#define FILE_MANAGER_EMULATOR_H

#include <fstream>
#include <memory>
#include <unordered_map>

#include "command_type.h"

class CommandParser;
class Logger;

/**
 * \brief ErrorCode represents possible execution outcomes of the FileManagerEmulator.
 */
enum class ErrorCode
{
    NoError = 0,            /// Execution completed successfully.
    CannotOpenDataStream,   /// Batch file could not be opened.
    CommandParsingError,    /// Invalid or unknown command syntax.
    CommandArgumentsError,  /// Incorrect number or type of command arguments.
    LogicError,             /// Runtime logic error during command execution.
    UknownException         /// Some exception was thrown
};

/**
 * \brief FileManagerEmulator (FME) emulates a virtual file system
 * supporting batch commands for directory and file manipulation.
 *
 * FME is a virtual, in-memory file system. It doesn't 
 * interact with the real disk but instead emulates file operations such as
 * creating, removing, copying, and moving files or directories.
 * FME can execute commands from the batch file or from standard input.
 * In the result of execution it outputs a formatted directory tree or
 * an error message if execution fails.
 */
class FileManagerEmulator final
{
    public:
        /**
         * \brief FsNode represents a single node (file or directory).
         */
        struct FsNode final
        {
            /**
             * \brief Creates a deep copy of this node and its children.
             * 
             * \param newName Optional new name for the root of the copied subtree.
             * \return A unique_ptr to the new node.
             */
            std::unique_ptr<FsNode> copy(std::string_view newName = "") const;

            std::string                                              name;
            bool                                                     isDirectory = true;
            std::unordered_map<std::string, std::unique_ptr<FsNode>> children;
        };

        /**
         * \brief NodeTransferMode defines whether a node is copied or moved between directories.
         */
        enum class NodeTransferMode
        {
            Copy,
            Move
        };

        /**
         * \brief NodeType defines the type of node.
         */
        enum class NodeType
        {
            Directory,
            File,
            Invalid
        };

        /**
         * \brief PathInfo decomposes a normalized absolute path into components.
         */
        struct PathInfo
        {
            std::string path;
            std::string basename;
            NodeType    type;  /// A "rough" guess about the file type based on the presence of '.',
                               /// since dirs can also have '.' in their names.
        };

    public:
        /**
         * \brief Constructs a FileManagerEmulator instance.
         * 
         * \param logger Custom logger implementation (optional). If null, a default Logger is created.
         */
        FileManagerEmulator(std::unique_ptr<Logger> logger);
        FileManagerEmulator(const FileManagerEmulator&) = delete;
        FileManagerEmulator(FileManagerEmulator&&)      = delete;

        ~FileManagerEmulator();

        FileManagerEmulator& operator=(const FileManagerEmulator&) = delete;
        FileManagerEmulator& operator=(FileManagerEmulator&&)      = delete;

        /**
         * \brief Prints the current virtual file tree in human-readable form
         * organized in alphabetical ascending order.
         */
        void      printFileTree() const;
        /**
         * \brief Runs a batch command file or reads commands from stdin.
         * 
         * \param batchFilePath Path to the batch file (empty string means stdin).
         * \return ErrorCode representing execution result.
         */
        ErrorCode run(std::string_view batchFilePath = "");

        /**
         * \brief Copies a file or directory (recursively) to a new location.
         */
        bool cp(std::string_view source, std::string_view destination);
        /**
         * \brief Creates a new directory at the given absolute path.
         */
        bool md(std::string_view dirAbsolutePath);
        /**
         * \brief Creates a new file at the given absolute path.
         */
        bool mf(std::string_view fileAbsolutePath);
        /**
         * \brief Moves a file or directory (recursively) to a new location.
         */
        bool mv(std::string_view source, std::string_view destination);
        /**
         * \brief Removes a file or directory (recursively).
         */
        bool rm(std::string_view path);

    private:
        /**
         * \brief Validates command arguments and dispatches command to the corresponding handler.
         */
        ErrorCode   executeCommand(const Command& command);
        /**
         * \brief Finds a node by normalized absolute path. Returns nullptr if not found or on errors.
         */
        FsNode*     findNodeByPath(std::string_view normalizedNodePath) const;
        /**
         * \brief Initializes command parser from batch file or standard input.
         */
        bool        initCommandParser(std::string_view batchFilePath);
        /**
         * \brief Checks if the given path/basename combination represents the root directory.
         */
        bool        isRootDirectory(std::string_view path, std::string_view basename) const;
        /**
         * \brief Returns a child node by name. Returns nullptr on errors.
         */
        FsNode*     getChildNode(const FsNode* node, const std::string& childName,
                                 std::string_view normalizedNodePath) const;
        /**
         * \brief Splits normalized absolute path into components and infers node type.
         */
        PathInfo    getNodePathInfo(std::string_view normalizedNodeAbsolutePath,
                                    NodeType         requiredNodeType = NodeType::Invalid) const;
        /**
         * \brief Normalizes a given path: trims components and merges redundant delimiters.
         */                           
        std::string normalizePath(std::string_view path) const;
        /**
         * \brief Transfers (copies/moves) node between directories.
         * 
         * \param requiredNodeType Type of transfered node.
         * \param parentS The parent node of the source node.
         * \param parentD The parent node of the destination node.
         * \param source The absolute path of the source.
         * \param destination The absolute path of the destination.
         * \param basenameS The name of the source node.
         * \param basenameD The name of the destination node.
         * \param pathD The path to the destination node without name of the file/directory.
         * \param ignoreIfAlreadyExist If true, the operation succeeds silently when a node with the same name
         * already exists in the target directory. If false, an existing node at the target path causes the operation to fail.
         * \param transferMode Marks how to handle the source node during transfer (Copy - don't touch, Move - remove).
         */
        bool        transferNode(NodeType requiredNodeType, FileManagerEmulator::FsNode* parentS,
                                 FileManagerEmulator::FsNode* parentD, std::string_view source, std::string_view destination,
                                 const std::string& basenameS, const std::string& basenameD, std::string_view pathD,
                                 bool ignoreIfAlreadyExist, NodeTransferMode transferMode);
        /**
         * \brief Validates node creation context and returns parent node if creation is allowed.
         * 
         * \see transferNode() for details about parameters.
         */
        FsNode*     validateNodeCreation(NodeType requiredNodeType, const PathInfo& pathInfo, std::string_view nodePath,
                                         bool ignoreIfAlreadyExist) const;
        /**
         * \brief Validates and performs a move/copy operation between paths.
         */
        bool        validateAndTransferNode(std::string_view source, std::string_view destination,
                                            NodeTransferMode transferMode);
        /**
         * \brief Validates number of arguments for a command.
         */
        bool        validateNumberOfCommandArguments(const Command& command) const;

    private:
        std::unique_ptr<Logger>        m_logger;
        std::ifstream                  m_fileInStream;
        std::unique_ptr<FsNode>        m_fsRoot;
        std::unique_ptr<CommandParser> m_parser;
};

#endif  // FILE_MANAGER_EMULATOR_H
