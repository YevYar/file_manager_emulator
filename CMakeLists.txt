cmake_minimum_required(VERSION 3.12.0)
project(file_manager_emulator VERSION 0.1.0 LANGUAGES CXX)

add_executable(file_manager_emulator src/main.cpp)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
target_compile_features(file_manager_emulator PUBLIC cxx_std_20)

# Strictly follow the standard
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(file_manager_emulator PRIVATE
        -Wall -Wextra -Wpedantic -pedantic-errors
    )
endif()