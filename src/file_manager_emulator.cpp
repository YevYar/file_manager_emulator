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

std::string formatPathErrorMsg(const std::string_view normalizedNodePath, const std::string_view errorMsg)
{
    return std::format("Invalid path {}: {}", normalizedNodePath, errorMsg);
};

std::string formatInvalidFileReferenceErrorMsg(const std::string_view path, const std::string_view basename)
{
    // Wrong basename of the file. Files cannot be referenced with / in the end.
    return formatPathErrorMsg(path, std::format("the basename {} is not a valid file name.", basename));
};

std::string nodeTypeToString(const FileManagerEmulator::NodeType nodeType)
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

        auto sortedChildrenKeys = std::vector<std::string>{};
        sortedChildrenKeys.reserve(node->children.size());
        for (const auto& [childName, childPtr] : node->children)
        {
            if (childPtr)
            {
                sortedChildrenKeys.push_back(childName);
            }
        }

        std::sort(sortedChildrenKeys.begin(), sortedChildrenKeys.end(),
                  [](const auto& a, const auto& b)
                  {
                      return a < b;
                  });

        if (node->isDirectory)
        {
            for (const auto& childName : sortedChildrenKeys)
            {
                printNode(node->children.at(childName).get(), nextPrefix);
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

        if (m_fileInStream.is_open() && command.name != CommandName::Unknown)
        {
            m_logger->logInfo(std::format("Executing command [{}] ...", command.commandString));
        }

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

        if (!validateNumberOfCommandArguments(command))
        {
            return printResultTree(ErrorCode::CommandArgumentsError);
        }

        if (!executeCommand(command))
        {
            return printResultTree(ErrorCode::LogicError);
        }
    }

    return printResultTree(ErrorCode::NoError);
}

bool FileManagerEmulator::cp(const std::string_view source, const std::string_view destination)
{
    return validateAndTransferNode(source, destination, NodeTransferMode::Copy);
}

bool FileManagerEmulator::md(const std::string_view dirAbsolutePath)
{
    const auto normalizedDirPath = normalizePath(dirAbsolutePath);
    const auto nodePathInfo      = getNodePathInfo(normalizedDirPath, NodeType::Directory);
    const auto parent            = validateNodeCreation(NodeType::Directory, nodePathInfo, normalizedDirPath, false);

    if (parent)
    {
        auto newDir = std::unique_ptr<FsNode>{
          new FsNode{.name = nodePathInfo.basename, .isDirectory = true, .children = {}}
        };
        parent->children.insert({nodePathInfo.basename, std::move(newDir)});
        m_logger->logInfo(std::format("Directory {} is created.", normalizedDirPath));
    }

    return parent;
}

bool FileManagerEmulator::mf(const std::string_view fileAbsolutePath)
{
    const auto normalizedFilePath = normalizePath(fileAbsolutePath);
    const auto nodePathInfo       = getNodePathInfo(normalizedFilePath, NodeType::File);
    const auto parent             = validateNodeCreation(NodeType::File, nodePathInfo, normalizedFilePath, true);

    if (parent && !parent->children.contains(nodePathInfo.basename))
    {
        auto newFile = std::unique_ptr<FsNode>{
          new FsNode{.name = nodePathInfo.basename, .isDirectory = false, .children = {}}
        };
        parent->children.insert({nodePathInfo.basename, std::move(newFile)});
        m_logger->logInfo(std::format("File {} is created.", normalizedFilePath));
    }

    return parent;
}

bool FileManagerEmulator::mv(const std::string_view source, const std::string_view destination)
{
    return validateAndTransferNode(source, destination, NodeTransferMode::Move);
}

bool FileManagerEmulator::rm(const std::string_view absolutePath)
{
    const auto normalizedPath = normalizePath(absolutePath);
    const auto nodePathInfo   = getNodePathInfo(normalizedPath);
    const auto parent         = findNodeByPath(nodePathInfo.path);

    if (parent)
    {
        if (parent->children.contains(nodePathInfo.basename))
        {
            parent->children.erase(nodePathInfo.basename);
            m_logger->logInfo(std::format("The item {} is removed.", normalizedPath));
            return true;
        }
        else
        {
            m_logger->logError(std::format("No such item {}.", normalizedPath));
        }
    }

    return false;
}

bool FileManagerEmulator::executeCommand(const Command& command)
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

FileManagerEmulator::FsNode* FileManagerEmulator::findNodeByPath(const std::string_view normalizedNodePath) const
{
    const auto findNextDelimiter = [normalizedNodePath](const std::size_t startPos)
    {
        return normalizedNodePath.find_first_of(pathDelimiter, startPos);
    };

    auto startPos = std::size_t{0}, delimiterPos = findNextDelimiter(0);
    auto currentNode = m_fsRoot.get();
    auto nodeName    = std::string{};

    if (delimiterPos == std::string::npos)
    {
        nodeName = normalizedNodePath;
    }
    else
    {
        for (; delimiterPos != std::string::npos; delimiterPos = findNextDelimiter(startPos))
        {
            nodeName = normalizedNodePath.substr(startPos, delimiterPos - startPos);

            if (!nodeName.empty())
            {
                currentNode = getChildNode(currentNode, nodeName, normalizedNodePath);

                if (!currentNode)
                {
                    return nullptr;
                }
            }

            startPos = delimiterPos + 1;
        }

        nodeName = normalizedNodePath.substr(startPos, normalizedNodePath.length() - startPos);

        if (nodeName.empty() && !currentNode->isDirectory)
        {
            // File path with trailing slash is an invalid file reference.
            m_logger->logError(formatInvalidFileReferenceErrorMsg(normalizedNodePath, currentNode->name));
            return nullptr;
        }
    }

    return nodeName.empty() ? currentNode : getChildNode(currentNode, nodeName, normalizedNodePath);
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
            m_logger->logError(std::format("{}: {}. {}", batchFilePath, "Cannot open the batch file for reading",
                                           std::strerror(errno)));
            return false;
        }
    }
    else
    {
        m_parser = std::make_unique<CommandParser>(std::cin);
    }

    return m_parser != nullptr;
}

bool FileManagerEmulator::isRootDirectory(const std::string_view path, const std::string_view basename) const
{
    return path == m_fsRoot->name && basename.empty();
}

FileManagerEmulator::FsNode* FileManagerEmulator::getChildNode(const FsNode* node, const std::string& childName,
                                                               const std::string_view normalizedNodePath) const
{
    if (!node)
    {
        m_logger->logError(formatPathErrorMsg(normalizedNodePath, "null node is passed."));
        return nullptr;
    }
    if (!node->isDirectory)
    {
        m_logger->logError(formatPathErrorMsg(normalizedNodePath, node->name + " is not a directory."));
        return nullptr;
    }
    if (!node->children.contains(childName))
    {
        m_logger->logError(formatPathErrorMsg(normalizedNodePath,
                                              node->name + " does not contain the item " + childName + "."));
        return nullptr;
    }

    return node->children.at(childName).get();
}

FileManagerEmulator::PathInfo FileManagerEmulator::getNodePathInfo(std::string_view normalizedNodeAbsolutePath,
                                                                   const NodeType   requiredNodeType) const
{
    auto result = PathInfo{};

    if (normalizedNodeAbsolutePath.empty())
    {
        //  Empty path is considered as the root
        result.type = NodeType::Directory;
        result.path = pathDelimiter;
        return result;
    }

    const auto pathHasTrailingSlash = normalizedNodeAbsolutePath.back() == pathDelimiter;
    if (pathHasTrailingSlash)
    {
        normalizedNodeAbsolutePath.remove_suffix(1);
    }

    // Find last slash
    const auto pos = normalizedNodeAbsolutePath.find_last_of(pathDelimiter);

    if (pos == std::string::npos)
    {
        // No slash -> everything is basename, path is root
        result.path     = pathDelimiter;
        result.basename = normalizedNodeAbsolutePath;
    }
    else if (pos == 0)
    {
        // Path is root "/"
        result.path     = pathDelimiter;
        result.basename = normalizedNodeAbsolutePath.substr(1);
    }
    else
    {
        result.path     = normalizedNodeAbsolutePath.substr(0, pos);
        result.basename = normalizedNodeAbsolutePath.substr(pos + 1);
    }

    if (pathHasTrailingSlash && requiredNodeType == NodeType::File)
    {
        // For example, /d1/f1.t/ is Invalid, if f1 is a file
        //              /d1/f1/ is Invalid too, if f1 is a file
        result.basename += pathDelimiter;
        result.type      = NodeType::Invalid;
    }
    else
    {
        // This is a "rough" guess about the file type based on the presence of '.',
        // since dirs can also have '.' in their names.
        result.type = isFilename(result.basename) ? NodeType::File : NodeType::Directory;
    }

    if (result.path.empty())
    {
        result.path = pathDelimiter;
    }

    return result;
}

std::string FileManagerEmulator::normalizePath(std::string_view nodePath) const
{
    // "//", "/   /" etc. inside of the path are considered as the current node.
    // "dir1//dir2" and "dir1/   /dir2" are valid path and result is "dir1/dir2".

    if (nodePath.empty())
    {
        return std::string{pathDelimiter};
    }

    auto result = std::string{};
    result.reserve(nodePath.size() + 1);

    const auto findNextDelimiter = [nodePath](const std::size_t startPos)
    {
        return nodePath.find_first_of(pathDelimiter, startPos);
    };

    auto startPos = std::size_t{0}, delimiterPos = findNextDelimiter(0);
    auto nodeName = std::string{};

    if (delimiterPos == std::string::npos)
    {
        // No slash -> everything is basename, path is root
        result = std::string{nodePath};
        trim(result);
        result = pathDelimiter + result;
    }
    else
    {
        for (; delimiterPos != std::string::npos; delimiterPos = findNextDelimiter(startPos))
        {
            nodeName = std::string{nodePath.substr(startPos, delimiterPos - startPos)};
            trim(nodeName);

            if (!nodeName.empty())
            {
                result.append(pathDelimiter + nodeName);
            }

            startPos = delimiterPos + 1;
        }

        nodeName = std::string{nodePath.substr(startPos, nodePath.length() - startPos)};
        trim(nodeName);

        if (nodeName.empty())
        {
            // If entered path contains trailing /, we want to keep it
            result.push_back(pathDelimiter);
        }
        else
        {
            result.append(pathDelimiter + nodeName);
        }
    }

    return result;
}

bool FileManagerEmulator::transferNode(const NodeType requiredNodeType, FileManagerEmulator::FsNode* const parentS,
                                       FileManagerEmulator::FsNode* const parentD, const std::string_view source,
                                       const std::string_view destination, const std::string& basenameS,
                                       const std::string& basenameD, const std::string_view pathD,
                                       const bool ignoreIfAlreadyExist, const NodeTransferMode transferMode)
{
    // If destination is "/" (no basename), we must move in the root with the current name.
    const auto nameAfterTransfer = basenameD.empty() ? basenameS : basenameD;
    const auto destinationPath   = basenameD.empty() ? destination : pathD;  // path is destination without basename
    const auto nodeTypeStr       = nodeTypeToString(requiredNodeType);

    if (!parentD->isDirectory)
    {
        m_logger
          ->logError(std::format("Cannot move source {} {} in destination {} because destination is not a "
                                 "directory.",
                                 nodeTypeStr, source, destinationPath));
        return false;
    }

    if (!parentD->children.contains(nameAfterTransfer))
    {
        if (transferMode == NodeTransferMode::Move)
        {
            parentD->children.insert({nameAfterTransfer, std::move(parentS->children.at(basenameS))});
            parentD->children.at(nameAfterTransfer)->name = nameAfterTransfer;
            parentS->children.erase(basenameS);
            m_logger->logInfo(std::format("The {} {} is moved in {} with name {}.", nodeTypeStr, source,
                                          destinationPath, nameAfterTransfer));
        }
        else
        {
            auto newNode = parentS->children.at(basenameS)->copy(nameAfterTransfer);
            parentD->children.insert({nameAfterTransfer, std::move(newNode)});
            m_logger->logInfo(std::format("The {} {} is copied in {} with name {}.", nodeTypeStr, source,
                                          destinationPath, nameAfterTransfer));
        }
        return true;
    }
    else
    {
        if (ignoreIfAlreadyExist)
        {
            m_logger
              ->logInfo(std::format("Ignore move of {} {} in {} because the item with such a name already exists in "
                                    "the {}.",
                                    nodeTypeStr, source, destinationPath, destinationPath));
            return true;
        }
        else
        {
            m_logger
              ->logError(std::format("Cannot move {} {} in {} because the item with such a name already exists in {}.",
                                     nodeTypeStr, source, destinationPath, destinationPath));
            return false;
        }
    }
    return false;
}

FileManagerEmulator::FsNode* FileManagerEmulator::validateNodeCreation(const NodeType         requiredNodeType,
                                                                       const PathInfo&        pathInfo,
                                                                       const std::string_view nodePath,
                                                                       const bool ignoreIfAlreadyExist) const
{
    const auto& [path, basename, nodeType] = pathInfo;

    if (requiredNodeType == NodeType::File && nodeType == NodeType::Invalid)
    {
        // File can have basename without '.'
        // Directory can have basename with '.'
        // Wrong is file "f.txt/" or "f/"
        m_logger->logError(formatInvalidFileReferenceErrorMsg(nodePath, basename));
        return nullptr;
    }

    const auto parent = findNodeByPath(path);
    if (!parent)
    {
        return nullptr;
    }
    if (!parent->isDirectory)
    {
        m_logger->logError(formatPathErrorMsg(nodePath, std::format("{} is not a directory.", parent->name)));
        return nullptr;
    }
    if (parent->children.contains(basename))
    {
        const auto nodeTypeStr = nodeTypeToString(requiredNodeType);

        if (ignoreIfAlreadyExist)
        {
            m_logger
              ->logInfo(std::format("Ignore creation of the {} {} because the item with such a name already exists.",
                                    nodeTypeStr, nodePath));
        }
        else
        {
            m_logger->logError(std::format("Cannot create {} {}: parent directory {} already contains {} {}.",
                                           nodeTypeStr, nodePath, path, nodeTypeStr, basename));
            return nullptr;
        }
    }

    return parent;
}

bool FileManagerEmulator::validateAndTransferNode(const std::string_view s, const std::string_view d,
                                                  const NodeTransferMode transferMode)
{
    const auto source = normalizePath(s), destination = normalizePath(d);
    const auto& [pathS, basenameS, nodeTypeS] = getNodePathInfo(source);
    const auto& [pathD, basenameD, nodeTypeD] = getNodePathInfo(destination);
    const auto destinationIsRoot              = isRootDirectory(pathD, basenameD);
    // mv d1/d2 /   - basenameD is empty, so we say that newBasenameD = basenameS
    const auto newBasenameD                   = destinationIsRoot ? basenameS : basenameD;

    if (isRootDirectory(pathS, basenameS))
    {
        m_logger->logError("Cannot move the root directory.");
        return false;
    }
    if ((pathS == pathD && basenameS == basenameD) || (pathS == std::string{pathDelimiter} && destinationIsRoot))
    {
        // m_logger->logError(std::string{"Cannot move the item into itself."});
        // return false;

        // Ignore moving item into itself.
        return true;
    }
    if (destination.starts_with(source) && destination.length() > source.length()
        && destination.at(source.length()) == pathDelimiter)
    {
        // Checks that, for example, /d1 is a subdirectory of /d1/d2 and not a subdirectory of /d11/d2
        m_logger->logError(std::format("The element {} cannot be moved into own subdirectory {}.", source,
                                       destination));
        return false;
    }

    const auto parentS = findNodeByPath(pathS);
    if (!parentS)
    {
        return false;
    }
    if (!parentS->children.contains(basenameS))
    {
        m_logger->logError(std::format("No such {} {}.", nodeTypeToString(nodeTypeS), source));
        return false;
    }

    auto parentD = findNodeByPath(pathD);
    if (!parentD)
    {
        return false;
    }
    if (parentS == parentD && basenameS == newBasenameD)
    {
        return true;
    }
    if (!parentD->isDirectory)
    {
        m_logger
          ->logError(std::format("Cannot move the item {} in destination {} because destination is not a "
                                 "directory.",
                                 source, pathD));
        return false;
    }

    const auto sourceIsDir = parentS->children.at(basenameS)->isDirectory;
    if (!sourceIsDir)
    {
        if (source.back() == pathDelimiter)
        {
            // Wrong basename of the source file.
            m_logger->logError(formatInvalidFileReferenceErrorMsg(source, basenameS + pathDelimiter));
            return false;
        }
    }

    const auto requiredNodeType     = sourceIsDir ? NodeType::Directory : NodeType::File;
    const auto ignoreIfAlreadyExist = sourceIsDir ? false : true;

    if (parentD->children.contains(newBasenameD) && !destinationIsRoot)
    {
        // For example, we have d3/d1. After we mv d3/d1 /  .
        // This must move d1 from d3 into the root folder.
        // If basenameD == newBasenameD -> the destination path is like /d1 - move d3/d1 into /d1
        // If basenameD != newBasenameD -> the destination path is like / - move d3/d1 into /
        // So, in case basenameD != newBasenameD we must prevent replacing of parent root with
        // it child d1.
        parentD = parentD->children.at(newBasenameD).get();

        // Move with the same name
        return transferNode(requiredNodeType, parentS, parentD, source, destination, basenameS, "", pathD,
                            ignoreIfAlreadyExist, transferMode);
    }
    else
    {
        if (!sourceIsDir && destination.back() == pathDelimiter && !destinationIsRoot)
        {
            // Wrong basename of the destination file.
            m_logger->logError(formatInvalidFileReferenceErrorMsg(destination, basenameD + pathDelimiter));
            return false;
        }

        // Move and rename
        return transferNode(requiredNodeType, parentS, parentD, source, destination, basenameS, newBasenameD, pathD,
                            ignoreIfAlreadyExist, transferMode);
    }
    return false;
}

bool FileManagerEmulator::validateNumberOfCommandArguments(const Command& command) const
{
    if (command.name == CommandName::Unknown)
    {
        return false;
    }

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

std::unique_ptr<FileManagerEmulator::FsNode> FileManagerEmulator::FsNode::copy(const std::string_view newName) const
{
    auto newNode         = std::make_unique<FsNode>();
    newNode->name        = newName.empty() ? name : newName;
    newNode->isDirectory = isDirectory;

    for (const auto& [childName, childPtr] : children)
    {
        if (childPtr)
        {
            newNode->children[childName] = childPtr->copy();
        }
    }

    return newNode;
}
