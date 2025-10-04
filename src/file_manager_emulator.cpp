#include "file_manager_emulator.h"

#include <cstring>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>

#include "command_parser.h"
#include "helpers.h"
#include "logger.h"

namespace
{
constexpr inline auto pathDelimiter = '/';
constexpr inline auto fileDelimiter = '.';

bool isFilename(const std::string_view filename)
{
    return filename.find_first_of(fileDelimiter) != std::string_view::npos;
}

std::string nodeTypeToString(FileManagerEmulator::NodeType nodeType)
{
    if (nodeType == FileManagerEmulator::NodeType::Invalid)
    {
        return "invalid";
    }
    return nodeType == FileManagerEmulator::NodeType::Directory ? "directory" : "file";
}

}  // namespace

FileManagerEmulator::FileManagerEmulator(std::unique_ptr<Logger> logger) :
    m_logger{logger ? std::move(logger) : std::make_unique<Logger>()},
    m_fsRoot{new FsNode{.name = std::string{pathDelimiter}, .isDirectory = true, .children = {}}}
{
}

FileManagerEmulator::~FileManagerEmulator()
{
    if (m_fileInStream.is_open())
    {
        m_fileInStream.close();
    }
}

void FileManagerEmulator::printFileTree() const
{
    // m_logger.info The FileManagerEmulator is finished with/without error. Result file tree:
    auto output = std::string{"The FME file tree:\n"};

    const std::function<void(const FsNode*, const std::string&)> printNode =
      [&](const FsNode* node, const std::string& prefix)
    {
        auto       nextPrefix       = prefix;
        const auto nodeTypeShortStr = node->isDirectory ? "  [D]" : "  [F]";

        if (prefix.empty())
        {
            output     += node->name + nodeTypeShortStr + "\n";
            nextPrefix  = "|";
        }
        else
        {
            output     += prefix + "_" + node->name + nodeTypeShortStr + "\n";
            nextPrefix += " |";
        }

        if (node->isDirectory)
        {
            for (const auto& [childName, childPtr] : node->children)
            {
                if (childPtr)
                {
                    printNode(childPtr.get(), nextPrefix);
                }
            }
        }
    };

    printNode(m_fsRoot.get(), "");
    m_logger->logInfo(output);
}

ErrorCode FileManagerEmulator::run(const std::string_view batchFilePath)
{
    if (!initCommandParser(batchFilePath))
    {
        return ErrorCode::CannotOpenDataStream;
    }

    const auto printResultTree = [this](ErrorCode code)
    {
        if (code == ErrorCode::NoError)
        {
            m_logger->logInfo("FileManagerEmulator::run() is over without error.");
        }
        else
        {
            m_logger->logWarning("FileManagerEmulator::run() is over with error.");
        }
        printFileTree();
        return code;
    };

    while (m_parser->hasMoreInput())
    {
        const auto command = m_parser->getNextCommand();

        if (command.name == CommandName::Unknown)
        {
            m_logger->logError(command.error.has_value() ? command.error.value() : "Uknown command is met.");
            return printResultTree(ErrorCode::CommandParsingError);
        }
        else if (command.error.has_value())
        {
            m_logger->logError(command.error.value(), command.commandString);
            return printResultTree(ErrorCode::CommandParsingError);
        }

        // auto currentError = std::string{};

        if (!validateNumberOfCommandArguments(command /*, currentError*/))
        {
            // m_logger. error
            return printResultTree(ErrorCode::CommandArgumentsError);
        }

        if (!executeCommand(command /*, currentError*/))
        {
            // m_logger. error
            return printResultTree(ErrorCode::LogicError);
        }
    }

    return printResultTree(ErrorCode::NoError);
}

bool FileManagerEmulator::cp(std::string_view source, std::string_view destination)
{
    return false;
}

bool FileManagerEmulator::md(std::string_view dirAbsolutePath)
{
    const auto nodePathInfo = getNodePathInfo(dirAbsolutePath, NodeType::Directory);
    auto       parent       = validateNodeCreation(NodeType::Directory, nodePathInfo, dirAbsolutePath, false);

    if (parent)
    {
        auto newDir = std::unique_ptr<FsNode>{
          new FsNode{.name = nodePathInfo.basename, .isDirectory = true, .children = {}}
        };
        parent->children.insert({nodePathInfo.basename, std::move(newDir)});
        m_logger->logInfo(std::format("Directory {} is created.", dirAbsolutePath));
    }

    return parent;
}

bool FileManagerEmulator::mf(std::string_view fileAbsolutePath)
{
    const auto nodePathInfo = getNodePathInfo(fileAbsolutePath, NodeType::File);
    auto       parent       = validateNodeCreation(NodeType::File, nodePathInfo, fileAbsolutePath, true);

    if (parent && !parent->children.contains(nodePathInfo.basename))
    {
        auto newFile = std::unique_ptr<FsNode>{
          new FsNode{.name = nodePathInfo.basename, .isDirectory = false, .children = {}}
        };
        parent->children.insert({nodePathInfo.basename, std::move(newFile)});
        m_logger->logInfo(std::format("File {} is created.", fileAbsolutePath));
    }

    return parent;
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
    const auto findNextDelimiter = [nodePath](const std::size_t startPos)
    {
        return nodePath.find_first_of(pathDelimiter, startPos);
    };

    const auto formatPathErrorMsg = [nodePath](const std::string& errorMsg)
    {
        return std::format("Invalid path {}: {}", nodePath, errorMsg);
    };

    auto startPos = std::size_t{0}, delimiterPos = findNextDelimiter(0);
    auto currentNode = m_fsRoot.get();
    auto nodeName    = std::string{};

    if (delimiterPos == std::string::npos)
    {
        nodeName = std::string{nodePath};
        trim(nodeName);
    }
    else
    {
        for (; delimiterPos != std::string::npos; delimiterPos = findNextDelimiter(startPos))
        {
            nodeName = std::string{nodePath.substr(startPos, delimiterPos - startPos)};
            trim(nodeName);

            if (!nodeName.empty())
            {
                currentNode = getChildNode(currentNode, nodeName, outError);

                if (!currentNode)
                {
                    outError = formatPathErrorMsg(outError);
                    return nullptr;
                }
            }
            else
            {
                // "//", "/   /" etc. inside of the path are considered as the current node.
                // "dir1//dir2" and "dir1/   /dir2" are valid path.
            }

            startPos = delimiterPos + 1;
        }

        nodeName = std::string{nodePath.substr(startPos, nodePath.length() - startPos)};
        trim(nodeName);

        if (nodeName.empty() && !currentNode->isDirectory)
        {
            outError = std::format("{} is not a directory.", currentNode->name);
            outError = formatPathErrorMsg(outError);
            return nullptr;
        }
    }

    if (nodeName.empty())
    {
        return currentNode;
    }
    else
    {
        auto childNode = getChildNode(currentNode, nodeName, outError);

        if (!childNode)
        {
            outError = formatPathErrorMsg(outError);
        }

        return childNode;
    }

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

FileManagerEmulator::FsNode* FileManagerEmulator::getChildNode(const FsNode* node, const std::string& childName,
                                                               std::string& outError) const
{
    if (!node->isDirectory)
    {
        outError = node->name + " is not a directory.";
        return nullptr;
    }
    if (!node->children.contains(childName))
    {
        outError = node->name + " does not contain the item " + childName + ".";
        return nullptr;
    }

    return node->children.at(childName).get();
}

FileManagerEmulator::PathInfo FileManagerEmulator::getNodePathInfo(std::string_view nodeAbsolutePath,
                                                                   const NodeType   requiredNodeType) const
{
    auto result = PathInfo{};

    if (nodeAbsolutePath.empty())
    {
        //  Empty path can be considered as the root
        result.type = NodeType::Directory;
        result.path = pathDelimiter;
        return result;
    }

    // Remove trailing slashes
    auto trailingSlashesCounter = 0;

    while (nodeAbsolutePath.size() > 1
           && (nodeAbsolutePath.back() == pathDelimiter || isSpace(nodeAbsolutePath.back())))
    {
        if (nodeAbsolutePath.back() == pathDelimiter)
        {
            ++trailingSlashesCounter;
        }

        nodeAbsolutePath.remove_suffix(1);
    }

    auto trimmedPath = std::string{nodeAbsolutePath};
    trim(trimmedPath);

    // Find last slash
    const auto pos = trimmedPath.find_last_of(pathDelimiter);

    if (pos == std::string::npos)
    {
        // No slash -> everything is basename, path empty
        result.basename = trimmedPath;
        result.path     = pathDelimiter;
    }
    else if (pos == 0)
    {
        // Path is root "/"
        result.path     = std::string{pathDelimiter};
        result.basename = std::string{trimmedPath.substr(1)};
    }
    else
    {
        result.path     = std::string{trimmedPath.substr(0, pos)};
        result.basename = std::string{trimmedPath.substr(pos + 1)};
    }

    if ((trailingSlashesCounter > 0
         && (requiredNodeType == NodeType::File || nodeAbsolutePath.find_last_of(fileDelimiter) != std::string::npos)))
    {
        // For example, /d1/f1.t/ is Invalid
        //              /d1/f1/ is Invalid too, if f1 is a file
        result.basename += pathDelimiter;
        result.type      = NodeType::Invalid;
    }
    else
    {
        result.type = isFilename(result.basename) ? NodeType::File : NodeType::Directory;
    }

    trim(result.path);
    trim(result.basename);

    if (result.path.empty())
    {
        result.path = pathDelimiter;
    }

    return result;
}

FileManagerEmulator::FsNode* FileManagerEmulator::validateNodeCreation(NodeType               requiredNodeType,
                                                                       const PathInfo&        pathInfo,
                                                                       const std::string_view nodePath,
                                                                       bool ignoreIfAlreadyExist) const
{
    const auto [path, basename, nodeType] = pathInfo;

    if ((requiredNodeType == NodeType::Directory && nodeType != NodeType::Directory)
        || (requiredNodeType == NodeType::File && nodeType == NodeType::Invalid))
    {
        // File can have basename without .
        m_logger->logError(std::format("Invalid path {}: the basename {} is not a valid {} name.", nodePath, basename,
                                       nodeTypeToString(requiredNodeType)));
        return nullptr;
    }

    auto errorMessage = std::string{};
    auto parent       = findNodeByPath(path, errorMessage);

    if (!parent)
    {
        m_logger->logError(errorMessage);
        return nullptr;
    }
    if (!parent->isDirectory)
    {
        m_logger->logError(std::format("Invalid path {}: {} is not a directory.", nodePath, parent->name));
        return nullptr;
    }
    if (parent->children.contains(basename))
    {
        const auto nodeTypeStr = nodeTypeToString(requiredNodeType);

        if (!ignoreIfAlreadyExist)
        {
            m_logger->logError(std::format("Cannot create {} {}: parent directory {} already contains {} {}.",
                                           nodeTypeStr, nodePath, path, nodeTypeStr, basename));
            return nullptr;
        }
        else
        {
            m_logger->logInfo(std::format("Ignore creation of the {} {} because it already exists.", nodeTypeStr,
                                          nodePath));
        }
    }

    return parent;
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
        m_logger->logError(std::format("Command {} accepts {} argument(-s) (the number of passed arguments is {}).",
                                       m_parser->commandNameToString(command.name), numArgsToAccept, numPassedArgs),
                           command.commandString);
        return false;
    }

    return true;
}
