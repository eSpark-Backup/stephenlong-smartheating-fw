# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(EXTRA_COMPONENT_DIRS libraries)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Read the contents of defines.h
file(READ "${CMAKE_SOURCE_DIR}/main/defines.h" DEFINES_CONTENT)

# Extract the firmware version using regex
string(REGEX MATCH "#define[ \t]+FIRMWARE_VERSION[ \t]+\"([^\"]+)\"" _match "${DEFINES_CONTENT}")

# Extract just the version string
string(REGEX REPLACE "#define[ \t]+FIRMWARE_VERSION[ \t]+\"([^\"]+)\"" "\\1" PROJECT_VER "${_match}")

# Set the project version
message(STATUS "Extracted firmware version: ${PROJECT_VER}")

set(PROJECT_VER "${PROJECT_VER}")
project(Varo_Smart_Heater)
