#include <iostream>

#include "file_manager_emulator.h"
#include "logger.h"

int main(int argc, char** argv)
{
    std::cout << "File Manager Emulator is started!\n" << std::endl;

    const auto batchFileName = argc > 1 ? argv[1] : "";

    auto fme = FileManagerEmulator{std::make_unique<Logger>()};

    return static_cast<int>(fme.run(batchFileName));
}
