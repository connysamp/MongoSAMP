cmake_minimum_required(VERSION 3.10)
project(MongoSAMP CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# If vcpkg is used, this helps find packages
find_package(mongocxx CONFIG REQUIRED)
find_package(bsoncxx CONFIG REQUIRED)

# The SA:MP SDK directory must exist inside 'sdk'
include_directories(sdk/amx)
include_directories(sdk)

# All source files
set(SOURCES
    src/main.cpp
    src/Natives.cpp
    src/MongoSystem.cpp
    sdk/amxplugin.cpp
)

# Def file for MSVC to export standard SA:MP plugin functions
set(DEF_FILE src/plugin.def)

# Output as shared library (.dll or .so)
add_library(${PROJECT_NAME} SHARED ${SOURCES} ${DEF_FILE})

# Link MongoDB drivers
target_link_libraries(${PROJECT_NAME} PRIVATE mongo::mongocxx_shared mongo::bsoncxx_shared)

# Preprocessor definitions for SA:MP SDK
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32 _WINDOWS _CRT_SECURE_NO_WARNINGS HAVE_STDINT_H)
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE LINUX HAVE_STDINT_H)
endif()

# Compiler specific flags
if(MSVC)
    # Output to standard dll format without lib prefix
    set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
else()
    # Require 32-bit compilation on Linux (SA:MP is 32-bit)
    target_compile_options(${PROJECT_NAME} PRIVATE -m32)
    target_link_options(${PROJECT_NAME} PRIVATE -m32)
endif()
