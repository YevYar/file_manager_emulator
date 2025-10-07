# File Manager Emulator (FME)

File Manager Emulator (FME) emulates the details of creating, removing, copying and moving files and directories. FME is capable to read and execute commands from standard input as well as a batch file with different kind of commands. After the batch file execution it generates and prints out formatted directory structure or an error message if something went wrong to standard output. FME does nothing with the real file structure on local hard drives and only emulates these activities.
FME is written in C++20.

## Key features

### Commands

- md – make directory
- mf – make file
- rm – remove file/directory (recursive)
- cp – copy file/directory (recursive)
- mv – move file/directory (recursive)

### Commands' rules

- `md`, `mf`, `cp` and `mv` return error if the destination path contains not-existing nodes (not-existing intermediate nodes). They prevent creating child nodes for file nodes.
- Operations on directories return error if the destination has the node with the same name (`md /d1` will return error if d1 exists in the root directory).
- Operations on files ignores if the destination has the node with the same name (`mf /f1` will ignore the existing file/directory in the root directory).
- `mv` and `cp` ignore transfering the same node: `mv f1 f1` `mv f1 /`.
- `mv` and `cp` return error when transfering node into own subdirectory.

### Nodes naming and referencing

- Root directory is `/`.
- The name of a node must be unique on the current level (one node cannot contain several children with the same name). The type of nodes isn't considered.
- Name cannot be empty.
- Names are case sensitive: `f` and `F` are different nodes.
- `.` can be used in file as well as in directories name.
- Directories can be referenced in different ways:
```
# next commands create directories in the root
md d1
md /d2
md d3/
md /d4/
```
- Files can be referenced in different ways. However, trailing `/` is forbidden:
```
# next commands create files in the root
mf f1
mf /f2
mf f3/ # error
mf /f4/ # error
```

### Parsing

- Command Parser supports quoted arguments, whitespace handling, and descriptive error detection.
- `///////`, `/`, `/ / /` reference the root directory.
- Paths can contain whitespaces: `"/d1/d2 test"` is valid node with the name `d2 test` in the parent node `d1`.

### Logging

- Formatted output with levels (ERROR, INFO, WARNING).
- Command lines, wrong command names, paths and basenames are logged.
- The directory tree is printed in structured alphabetical ascending order:
```
INFO: The FME file tree:
/  [D]
|_f2  [F]
|_d1  [D]
|_d2  [D]
| |_d1  [D]
| | |_d11  [D]
```

## Installation

- Use CMake to generate project for desired build system.