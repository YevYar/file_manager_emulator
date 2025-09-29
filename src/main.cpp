#include <iostream>

#include "command_parser.h"

int main(int, char**)
{
    std::cout << "Hello, from file_manager_emulator!\n";

    auto Parser = CommandParser(std::cin);
    while (Parser.hasMoreInput())
    {
        const auto command = Parser.getNextCommand();
        std::cout << "arguments: ";
        for (const auto& arg : command.arguments)
        {
            std::cout << "[" << arg << "] ";
        }
        std::cout << std::endl;

        if (command.error.has_value())
        {
            std::cout << "error: " << command.error.value();
        }

        std::cout << "\n" << std::endl;
    }

    std::cout << "No more input. Finish..." << std::endl;

    return 0;
}
