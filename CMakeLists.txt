cmake_minimum_required(VERSION 3.25)
project(CrispRustGUI VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(CRISPGUI_BUILD_TESTS "Build the unit tests" ON)
set(CRISPGUI_EUI_NEO_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../Learn/sources/github.com/sudoevolve/EUI-NEO/v0.6.1"
    CACHE PATH "EUI-NEO v0.6.1 snapshot in the shared Learn library (read-only)")
if(NOT EXISTS "${CRISPGUI_EUI_NEO_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "EUI-NEO was not found at ${CRISPGUI_EUI_NEO_DIR}. Set CRISPGUI_EUI_NEO_DIR to the "
        "v0.6.1 snapshot (docs/ENVIRONMENT.md, section 4.1).")
endif()

# EUI-NEO options (docs/ENVIRONMENT.md, section 4.2): never download; X11 only on Linux.
set(EUI_DEPS_MODE bundled CACHE STRING "EUI-NEO dependency mode" FORCE)
set(FETCHCONTENT_FULLY_DISCONNECTED ON CACHE BOOL "Never download dependencies" FORCE)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "Build GLFW's Wayland backend" FORCE)
    set(GLFW_BUILD_X11 ON CACHE BOOL "Build GLFW's X11 backend" FORCE)
endif()
if(MINGW)
    # Self-contained Windows executables, tests included.
    add_link_options(-static)
endif()
add_subdirectory("${CRISPGUI_EUI_NEO_DIR}" "${CMAKE_BINARY_DIR}/eui-neo" EXCLUDE_FROM_ALL SYSTEM)

# Warnings, and the rule that application code uses neither exceptions nor RTTI
# (docs/CONVENTIONS.md), for every target built from this project's sources.
function(crisp_apply_options target)
    target_compile_options(${target} PRIVATE -Wall -Wextra -fno-exceptions -fno-rtti)
endfunction()

# Platform-neutral core and the platform seam; no UI dependency, so tests link it directly.
add_library(crisp_core STATIC
    src/app/app_data.cpp
    src/util/string_table.cpp)
if(WIN32)
    target_sources(crisp_core PRIVATE src/platform/paths_win32.cpp)
else()
    target_sources(crisp_core PRIVATE src/platform/paths_posix.cpp)
endif()
target_include_directories(crisp_core PUBLIC src)
crisp_apply_options(crisp_core)

# Application state and pages, built on EUI-NEO.
add_library(crisp_ui STATIC
    src/app/application.cpp
    src/ui/pages.cpp)
target_link_libraries(crisp_ui PUBLIC crisp_core eui::neo)
crisp_apply_options(crisp_ui)

# The executable. eui_neo_configure_app() adds EUI-NEO's entry point, icon, link options, and
# assets copy, so the warnings stay on this project's main.cpp only.
add_executable(crisprustgui src/main.cpp)
target_link_libraries(crisprustgui PRIVATE crisp_ui)
eui_neo_configure_app(crisprustgui)
set_source_files_properties(src/main.cpp PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra")
target_compile_options(crisprustgui PRIVATE -fno-exceptions -fno-rtti)

if(CRISPGUI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
