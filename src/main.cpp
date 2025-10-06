#include <iostream>

#include "file_manager_emulator.h"
#include "logger.h"

int main(int, char**)
{
    std::cout << "File Manager Emulator is started!\n" << std::endl;

    auto fme = FileManagerEmulator{std::make_unique<Logger>()};
    return static_cast<int>(fme.run(""));
}
