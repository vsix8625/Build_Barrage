cmake_minimum_required(VERSION 3.25)

project(barr VERSION 0.23.1 LANGUAGES C)

set(TARGET_NAME "barr-cmake") 

# ------------------------------
# Compiler settings
# ------------------------------
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE})

file(GLOB_RECURSE BARR_SOURCES
    ${CMAKE_SOURCE_DIR}/src/*.c
)

# Exclude external tools or build artifacts
list(FILTER BARR_SOURCES EXCLUDE REGEX ".*/tools/.*")
list(FILTER BARR_SOURCES EXCLUDE REGEX ".*/build/.*")

add_executable(${TARGET_NAME} ${BARR_SOURCES})

target_include_directories(${TARGET_NAME}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/barr_build_system
        ${CMAKE_SOURCE_DIR}/src/barr_commands
        ${CMAKE_SOURCE_DIR}/src/barr_platform/linux
        ${CMAKE_SOURCE_DIR}/src/olmos
)

# ------------------------------
# Libraries
# ------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(XXHASH REQUIRED libxxhash)

target_include_directories(${TARGET_NAME} PRIVATE ${XXHASH_INCLUDE_DIRS})
target_link_directories(${TARGET_NAME} PRIVATE ${XXHASH_LIBRARY_DIRS})
target_link_libraries(${TARGET_NAME} PRIVATE ${XXHASH_LIBRARIES} pthread)

# ------------------------------
# Compiler flags
# ------------------------------
target_compile_options(${TARGET_NAME} PRIVATE
    -Wall
    -Wextra
    -Werror
    -pthread
)

target_link_options(${TARGET_NAME} PRIVATE -Wl,--gc-sections)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(${TARGET_NAME} PRIVATE -g -O0)
    target_compile_definitions(${TARGET_NAME} PRIVATE BARR_DEBUG _GNU_SOURCE)
else()
    target_compile_options(${TARGET_NAME} PRIVATE
        -O0
		-mavx
        # -march=native
        # -fomit-frame-pointer
        # -funroll-loops
        # -fno-common
    )
    target_compile_definitions(${TARGET_NAME} PRIVATE NDEBUG _GNU_SOURCE BARR_RELEASE_BUILD)
endif()

