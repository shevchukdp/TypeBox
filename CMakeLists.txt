cmake_minimum_required(VERSION 3.16)

# Single source of truth for the version: the VERSION file.
# CI enforces that git tag vX.Y.Z matches this file before releasing.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" TYPEBOX_VERSION_RAW)
string(STRIP "${TYPEBOX_VERSION_RAW}" TYPEBOX_VERSION)

project(TypeBox VERSION ${TYPEBOX_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(TypeBox WIN32 src/main.cpp)

# Version token is stringified in main.cpp via TB_STR(), keep it unquoted here.
target_compile_definitions(TypeBox PRIVATE
    UNICODE _UNICODE
    TYPEBOX_VERSION=${TYPEBOX_VERSION})

target_link_libraries(TypeBox PRIVATE
    user32 shell32 shlwapi gdi32 comctl32 winmm)

# Example plugin (TypeAndRun ABI) — ships in the release zip under Plugins\.
add_library(calc SHARED examples/plugin_calc/calc.c)
target_link_libraries(calc PRIVATE user32)
set_target_properties(calc PROPERTIES PREFIX "")

if(MSVC)
    target_compile_options(TypeBox PRIVATE /W4)
else()
    target_compile_options(TypeBox PRIVATE -Wall -Wextra)
endif()
