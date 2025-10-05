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
    const auto normalizedDirPath = normalizePath(dirAbsolutePath);
    const auto nodePathInfo      = getNodePathInfo(normalizedDirPath, NodeType::Directory);
    auto       parent            = validateNodeCreation(NodeType::Directory, nodePathInfo, normalizedDirPath, false);

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

bool FileManagerEmulator::mf(std::string_view fileAbsolutePath)
{
    const auto normalizedFilePath = normalizePath(fileAbsolutePath);
    const auto nodePathInfo       = getNodePathInfo(normalizedFilePath, NodeType::File);
    auto       parent             = validateNodeCreation(NodeType::File, nodePathInfo, normalizedFilePath, true);

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

bool FileManagerEmulator::mv(std::string_view s, std::string_view d)
{
    const auto source = normalizePath(s), destination = normalizePath(d);
    const auto [pathS, basenameS, nodeTypeS] = getNodePathInfo(source);
    const auto [pathD, basenameD, nodeTypeD] = getNodePathInfo(destination);
    // mv d1/d2 /   - basenameD is empty, so we say that newBasenameD = basenameS
    const auto newBasenameD                  = basenameD.empty() ? basenameS : basenameD;

    if (isRootDirectory(pathS, basenameS))
    {
        m_logger->logError(std::string{"Cannot move the root directory."});
        return false;
    }
    if (pathS == pathD && basenameS == basenameD)
    {
        return true;
    }
    if (nodeTypeS == NodeType::Invalid)
    {
        m_logger->logError(std::string{"Invalid basename in the source path "} + std::string{source});
        return false;
    }
    if (nodeTypeD == NodeType::Invalid)
    {
        m_logger->logError(std::string{"Invalid basename in the destination path "} + std::string{destination});
        return false;
    }

    auto errorMsg = std::string{};
    auto parentS  = findNodeByPath(pathS, errorMsg);
    if (!parentS)
    {
        m_logger->logError(errorMsg);
        return false;
    }
    if (!parentS->children.contains(basenameS))
    {
        m_logger->logError(std::format("No such {} {}.", nodeTypeToString(nodeTypeS), source));
        return false;
    }

    // auto sourceNode = parentS->children.at(basenameS).get();

    auto parentD = findNodeByPath(pathD, errorMsg);
    if (!parentD)
    {
        m_logger->logError(errorMsg);
        return false;
    }
    if (parentS == parentD && basenameS == newBasenameD)
    {
        return true;
    }
    // if (!parentD->children.contains(newBasenameD))
    // {
    //     m_logger->logError(std::format("No such {} {}.", nodeTypeToString(nodeTypeD), destination));
    //     return false;
    // }

    if (parentS->children.at(basenameS)->isDirectory)
    {
        if (parentD->children.contains(newBasenameD))
        {
            // Move with the source directory basename to the destination.
            // Same as md, error if already exists.
            if (basenameD == newBasenameD)
            {
                // For example, we have /d1. After we mv d3/d1 /  .
                // This must move d1 from d3 into the root folder.
                // If basenameD == newBasenameD -> the destination path is like /d1 - move d3/d1 into /d1
                // If basenameD != newBasenameD -> the destination path is like / - move d3/d1 into /
                // So, in case basenameD != newBasenameD we must prevent replacing of parent root with
                // it child d1.
                parentD = parentD->children.at(newBasenameD).get();
            }
            if (!parentD->isDirectory)
            {
                m_logger
                  ->logError(std::format("Cannot move source directory {} in destination because destination is not a "
                                         "directory.",
                                         source));
                return false;
            }
            if (!parentD->children.contains(basenameS))
            {
                parentD->children.insert({basenameS, std::move(parentS->children.at(basenameS))});
                parentS->children.erase(basenameS);
                m_logger->logInfo(std::format("Directory {} is moved in {}.", source, destination));
            }
            else
            {
                m_logger->logError(std::format("Cannot move {} in {} because it already exists in {}.", source,
                                               destination, destination));
                return false;
            }
        }
        else
        {
            // Move and rename.

            if (!parentD->isDirectory)
            {
                m_logger
                  ->logError(std::format("Cannot move source directory {} in destination because destination is not a "
                                         "directory.",
                                         source));
                return false;
            }

            parentD->children.insert({newBasenameD, std::move(parentS->children.at(basenameS))});
            parentD->children.at(newBasenameD)->name = newBasenameD;
            parentS->children.erase(basenameS);
            m_logger->logInfo(std::format("Directory {} is moved to {} with name {}.", source, pathD, newBasenameD));
        }
    }
    else
    {
        const auto destinationIsDir = [&]()  // destinationIsDir
        {
            if (parentD->children.contains(newBasenameD))
            {
                return parentD->children.at(newBasenameD)->isDirectory;
            }
            return false;
        }();

        if (destinationIsDir)  // TODO  nodeTypeD == NodeType::Directory
        {
            if (basenameD == newBasenameD)
            {
                // For example, we have /d1. After we mv d3/d1 /  .
                // This must move d1 from d3 into the root folder.
                // If basenameD == newBasenameD -> the destination path is like /d1 - move d3/d1 into /d1
                // If basenameD != newBasenameD -> the destination path is like / - move d3/d1 into /
                // So, in case basenameD != newBasenameD we must prevent replacing of parent root with
                // it child d1.
                parentD = parentD->children.at(newBasenameD).get();
            }
            if (!parentD->isDirectory)
            {
                m_logger
                  ->logError(std::format("Cannot move source directory {} in destination because destination is not a "
                                         "directory.",
                                         source));
                return false;
            }
            // Move with the source file basename.
            // Same as mf, ignore if already exists.
            if (parentD->children.contains(basenameS))
            {
                m_logger->logInfo(std::format("Ignore move of {} in {} because it already exists in the destination.",
                                              source, destination));
            }
            else
            {
                parentD->children.insert({basenameS, std::move(parentS->children.at(basenameS))});
                parentS->children.erase(basenameS);
                m_logger->logInfo(std::format("File {} is moved in {}.", source, destination));
            }
        }
        else
        {
            if (!parentD->isDirectory)
            {
                m_logger
                  ->logError(std::format("Cannot move source directory {} in destination because destination is not a "
                                         "directory.",
                                         source));
                return false;
            }

            // Move and rename.
            // Same as mf, ignore if already exists.
            if (!parentD->children.contains(newBasenameD))
            {
                parentD->children.insert({newBasenameD, std::move(parentS->children.at(basenameS))});
                parentD->children.at(newBasenameD)->name = newBasenameD;
                parentS->children.erase(basenameS);
                m_logger->logInfo(std::format("File {} is moved in {} with name {}.", source, pathD, newBasenameD));
            }
            else
            {
                m_logger->logInfo(std::format("Ignore move of {} to {} because it already exists in the destination.",
                                              source, destination));
            }
        }
    }

    return true;
}

bool FileManagerEmulator::rm(std::string_view absolutePath)
{
    const auto normalizedPath = normalizePath(absolutePath);
    const auto [path, basename, nodeType] = getNodePathInfo(normalizedPath);
    auto       errorMessage   = std::string{};
    auto       parent         = findNodeByPath(path, errorMessage);

    if (parent)
    {
        if (parent->children.contains(basename))
        {
            parent->children.erase(basename);
            m_logger->logInfo(std::format("The {} {} is removed.", nodeTypeToString(nodeType),
                                          normalizedPath));
            return true;
        }
        else
        {
            m_logger->logError(std::format("No such {} {}.", nodeTypeToString(nodeType), normalizedPath));
        }
    }

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

FileManagerEmulator::FsNode* FileManagerEmulator::findNodeByPath(std::string_view normalizedNodePath,
                                                                 std::string&     outError) const
{
    const auto findNextDelimiter = [normalizedNodePath](const std::size_t startPos)
    {
        return normalizedNodePath.find_first_of(pathDelimiter, startPos);
    };
    const auto formatPathErrorMsg = [normalizedNodePath](const std::string& errorMsg)
    {
        return std::format("Invalid path {}: {}", normalizedNodePath, errorMsg);
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
                currentNode = getChildNode(currentNode, nodeName, outError);

                if (!currentNode)
                {
                    outError = formatPathErrorMsg(outError);
                    return nullptr;
                }
            }

            startPos = delimiterPos + 1;
        }

        nodeName = normalizedNodePath.substr(startPos, normalizedNodePath.length() - startPos);

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

bool FileManagerEmulator::isRootDirectory(std::string_view path, std::string_view basename) const
{
    return path == m_fsRoot->name && basename.empty();
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
        // No slash -> everything is basename, path empty
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

    if (pathHasTrailingSlash
        && (requiredNodeType
            == NodeType::File /*|| normalizedNodeAbsolutePath.find_last_of(fileDelimiter) != std::string::npos*/))
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
    // "dir1//dir2" and "dir1/   /dir2" are valid path.

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

FileManagerEmulator::FsNode* FileManagerEmulator::validateNodeCreation(NodeType               requiredNodeType,
                                                                       const PathInfo&        pathInfo,
                                                                       const std::string_view nodePath,
                                                                       bool ignoreIfAlreadyExist) const
{
    const auto [path, basename, nodeType] = pathInfo;

    if (/*(requiredNodeType == NodeType::Directory && nodeType != NodeType::Directory)
        || */
        (requiredNodeType == NodeType::File && nodeType == NodeType::Invalid))
    {
        // File can have basename without '.'
        // Directory can have basename with '.'
        // Wrong is file "f.txt/" or "f/"
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
