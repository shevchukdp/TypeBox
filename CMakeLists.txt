cmake_minimum_required(VERSION 3.16)
project(TypeBox LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(TypeBox WIN32 src/main.cpp)

target_compile_definitions(TypeBox PRIVATE UNICODE _UNICODE)

target_link_libraries(TypeBox PRIVATE
    user32 shell32 shlwapi gdi32 comctl32)
