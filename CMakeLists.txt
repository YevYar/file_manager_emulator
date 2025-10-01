cmake_minimum_required(VERSION 3.12.0)
project(file_manager_emulator VERSION 0.1.0 LANGUAGES CXX)

add_executable(file_manager_emulator)
target_sources(file_manager_emulator PRIVATE src/main.cpp src/command_parser.cpp include/command_parser.h
               include/command_type.h src/logger.cpp include/logger.h
               src/file_manager_emulator.cpp include/file_manager_emulator.h)
set(PATH_TO_INCLUDE ${file_manager_emulator_SOURCE_DIR}/include)
target_include_directories(file_manager_emulator PUBLIC ${PATH_TO_INCLUDE})

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
target_compile_features(file_manager_emulator PUBLIC cxx_std_20)

# Strictly follow the standard
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(file_manager_emulator PRIVATE
        -Wall -Wextra -Wpedantic -pedantic-errors
    )
endif()