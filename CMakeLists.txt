cmake_minimum_required(VERSION 3.16)

# Single source of truth for the version: the VERSION file.
# CI enforces that git tag vX.Y.Z matches this file before releasing.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" TYPEBOX_VERSION_RAW)
string(STRIP "${TYPEBOX_VERSION_RAW}" TYPEBOX_VERSION)

project(TypeBox VERSION ${TYPEBOX_VERSION} LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static C/C++ runtime for MSVC: the release zip must run without a VC++ redistributable.
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(TypeBox WIN32 src/main.cpp)

# Version token is stringified in main.cpp via TB_STR(), keep it unquoted here.
target_compile_definitions(TypeBox PRIVATE
    UNICODE _UNICODE
    TYPEBOX_VERSION=${TYPEBOX_VERSION})

target_link_libraries(TypeBox PRIVATE
    user32 shell32 shlwapi gdi32 comctl32 comdlg32 winmm winhttp bcrypt)

# Example plugin (TypeAndRun ABI) — ships in the release zip under Plugins\.
add_library(calc SHARED examples/plugin_calc/calc.c)
target_compile_definitions(calc PRIVATE CALC_BUILDING_DLL)
target_link_libraries(calc PRIVATE user32)
set_target_properties(calc PROPERTIES PREFIX "")

# First three plugins from docs/PLUGIN_GALLERY.md (v0.14.0 wave).
foreach(P units codec vol)
    string(TOUPPER ${P} PU)
    add_library(${P} SHARED examples/plugin_${P}/${P}.c)
    target_compile_definitions(${P} PRIVATE ${PU}_BUILDING_DLL)
    target_link_libraries(${P} PRIVATE user32)
    set_target_properties(${P} PROPERTIES PREFIX "")
endforeach()

# Gallery waves two+three (v0.16.0/v0.17.0): cliphist needs shell32 for its
# optional balloon; winlist (v0.17) is pure user32. taskman (v0.19: restart)
# needs shell32 for ShellExecuteW; pwgen pulls advapi32 for CryptGenRandom.
# timer (v0.21, gallery #7): winmm for the due-alert sound, gdi32 to paint its
# stopwatch mini-window, shell32 for the tray balloon.
# sysmon (v0.22, gallery #8): pure user32 (clipboards) + kernel32 stats calls.
# util (v0.23, RI3 #250/#251): ts + slug, user32 for the clipboard hand-off.
foreach(P cliphist taskman winlist pwgen timer sysmon util)
    string(TOUPPER ${P} PU)
    add_library(${P} SHARED examples/plugin_${P}/${P}.c)
    target_compile_definitions(${P} PRIVATE ${PU}_BUILDING_DLL)
    target_link_libraries(${P} PRIVATE user32)
    set_target_properties(${P} PROPERTIES PREFIX "")
endforeach()
target_link_libraries(cliphist PRIVATE shell32 gdi32)
target_link_libraries(taskman  PRIVATE shell32)
target_link_libraries(pwgen    PRIVATE advapi32)
target_link_libraries(timer    PRIVATE shell32 winmm gdi32)
target_link_libraries(util     PRIVATE bcrypt)

if(MSVC)
    # /utf-8: source files (i18n.h carries 20 language packs) are UTF-8; without
    # this flag MSVC decodes literals in the system ANSI codepage — the released
    # exe showed Cyrillic as mojibake while ASCII gates stayed green.
    target_compile_options(TypeBox PRIVATE /W4 /utf-8)
    target_compile_options(calc  PRIVATE /utf-8)
else()
    target_compile_options(TypeBox PRIVATE -Wall -Wextra)
    # Static runtime: portable exe, no libgcc/libstdc++ DLLs needed (ConEmu-grade portability).
    target_link_options(TypeBox PRIVATE -municode -static)
endif()
