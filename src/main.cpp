#include <iostream>
#include <memory>

#include "file_manager_emulator.h"
#include "logger.h"

int main(int, char**)
{
    std::cout << "Hello, from file_manager_emulator!\n";

    auto       fme        = FileManagerEmulator{std::make_unique<Logger>()};
    const auto returnCode = fme.run("/home/yevhenii/projects/file_manager_emulator/test_file.txt");
    // const auto returnCode = fme.run();
    return static_cast<int>(returnCode);
}
