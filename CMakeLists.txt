cmake_minimum_required(VERSION 3.28)
get_filename_component(ROOT_DIR_NAME ${CMAKE_SOURCE_DIR} NAME)
project(${ROOT_DIR_NAME} LANGUAGES C VERSION 0.14.1)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Please create and run from a build/ directory.")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

set(APP_NAME barr)

# Find xxHash library
find_library(XXHASH_LIBRARY NAMES xxhash libxxhash)
if(NOT XXHASH_LIBRARY)
    message(FATAL_ERROR "xxHash library not found. Please install libxxhash-dev (e.g., 'sudo apt install libxxhash-dev' on Ubuntu).")
endif()

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
    src/*.c
	src/olmos/*.c
    src/barr_commands/*.c
    src/barr_lua_rt/*.c
    src/barr_build_system/*.c
    src/barr_platform/*.c
    src/barr_platform/linux/*.c
)

add_executable(${APP_NAME} ${SRC_FILES})

target_include_directories(${APP_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/inc
	${CMAKE_SOURCE_DIR}/src/olmos
	${CMAKE_SOURCE_DIR}/src/barr_commands
	${CMAKE_SOURCE_DIR}/src/barr_build_system
	${CMAKE_SOURCE_DIR}/src/barr_platform
	${CMAKE_SOURCE_DIR}/src/barr_platform/linux
)

set(DEBUG_FLAGS -fuse-ld=lld -Wall -Wextra -Werror -MMD -MF -pedantic -ggdb -DDEBUG -DBARR_DEBUG -pthread)
set(RELEASE_FLAGS -Wall -Wextra -Werror -Ofast -march=native -flto -ffp-contract=fast -DNDEBUG -pthread)
target_compile_options(${APP_NAME} PRIVATE
    $<$<CONFIG:Debug>:${DEBUG_FLAGS}>
    $<$<CONFIG:Release>:${RELEASE_FLAGS}>
)

# Link xxHash library
target_link_libraries(${APP_NAME} PRIVATE ${XXHASH_LIBRARY})

if(DEFINED CMAKE_FLAGS_EXTRA)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_FLAGS_EXTRA}")
endif()

string(TOLOWER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_LOWER)
set(OUTPUT_BIN_DIR "${CMAKE_BINARY_DIR}/bin/${BUILD_TYPE_LOWER}")
set_target_properties(${APP_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_BIN_DIR}
)

# Write the executable path to .pounce/data/executable_path.pounce
set(EXECUTABLE_PATH_FILE "${CMAKE_SOURCE_DIR}/.pounce/data/executable_path.pounce")
add_custom_command(TARGET ${APP_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${APP_NAME}>"
    COMMAND ${CMAKE_COMMAND} -E echo "$<TARGET_FILE:${APP_NAME}>" > "${EXECUTABLE_PATH_FILE}"
    COMMENT "Writing executable path to ${EXECUTABLE_PATH_FILE}"
)

message(STATUS "-----------------------------------------------")
message(STATUS "Project: ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "C Flags: ${DEBUG_FLAGS}")
    message(STATUS "Extra Flags: ${CMAKE_FLAGS_EXTRA}")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "C Flags: ${RELEASE_FLAGS}")
    message(STATUS "Extra Flags: ${CMAKE_FLAGS_EXTRA}")
else()
    message(STATUS "C Flags: (unknown build type)")
endif()
message(STATUS "Sources: ${SRC_FILES}")
message(STATUS "Output: ${OUTPUT_BIN_DIR}/${APP_NAME}")
message(STATUS "Executable Path File: ${EXECUTABLE_PATH_FILE}")
message(STATUS "-----------------------------------------------")
file(WRITE "${CMAKE_SOURCE_DIR}/.pounce/data/target_type" "executable")

# ------------------------------------------------------------------------------------
# For build script stuff
# Write full path to build/bin_fullpath.barr (directory only)
set(BARR_BIN_PATH_FILE "${CMAKE_BINARY_DIR}/path_to_bin_dir.barr")
set(BARR_LOCK_FILE "${CMAKE_BINARY_DIR}/barr_install.lock")

add_custom_command(TARGET ${APP_NAME} POST_BUILD
    # Ensure the target dir exists
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${APP_NAME}>"
    # Write the directory containing the binary, not the binary itself
	COMMAND ${CMAKE_COMMAND} -E echo "$<TARGET_FILE_DIR:${APP_NAME}>" > "${BARR_BIN_PATH_FILE}"
    # Write lock file with version info
	COMMAND ${CMAKE_COMMAND} -E echo "version=${PROJECT_VERSION}" > "${BARR_LOCK_FILE}"
	COMMENT "Wrote BARR bin directory to ${BARR_BIN_PATH_FILE}, lock file ${BARR_LOCK_FILE}"
)

message(STATUS "Executable Path File (directory): ${BARR_BIN_PATH_FILE}")
message(STATUS "Install Lock File:                ${BARR_LOCK_FILE}")
