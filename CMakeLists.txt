cmake_minimum_required(VERSION 3.14)
project(shellpower C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# =============================================================================
# Version Configuration
# =============================================================================
# Get version from git tags
execute_process(
    COMMAND git describe --tags --abbrev=0
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

# Default version if git tag not available
if(NOT GIT_TAG)
    set(GIT_TAG "v0.0.0")
endif()

# Parse version components (strip leading 'v')
string(REGEX REPLACE "^v" "" VERSION_STRING "${GIT_TAG}")
string(REPLACE "." ";" VERSION_LIST ${VERSION_STRING})
list(LENGTH VERSION_LIST VERSION_LIST_LENGTH)
if(VERSION_LIST_LENGTH GREATER_EQUAL 3)
    list(GET VERSION_LIST 0 SHELLPOWER_VERSION_MAJOR)
    list(GET VERSION_LIST 1 SHELLPOWER_VERSION_MINOR)
    list(GET VERSION_LIST 2 SHELLPOWER_VERSION_PATCH)
else()
    set(SHELLPOWER_VERSION_MAJOR 0)
    set(SHELLPOWER_VERSION_MINOR 0)
    set(SHELLPOWER_VERSION_PATCH 0)
endif()
set(SHELLPOWER_VERSION "${VERSION_STRING}")

message(STATUS "Building shellpower version ${SHELLPOWER_VERSION}")

# =============================================================================
# GitHub Repository Info (from git remote)
# =============================================================================
execute_process(
    COMMAND git remote get-url origin
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_REMOTE_URL
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

# Parse owner/repo from URL (handles https and ssh formats)
# https://github.com/owner/repo.git or git@github.com:owner/repo.git
if(GIT_REMOTE_URL)
    # Try HTTPS format first: https://github.com/owner/repo.git
    string(REGEX MATCH "github\\.com[:/]([^/]+)/([^/\\.]+)" GITHUB_MATCH "${GIT_REMOTE_URL}")
    if(GITHUB_MATCH)
        string(REGEX REPLACE "github\\.com[:/]([^/]+)/([^/\\.]+)" "\\1" SHELLPOWER_GITHUB_OWNER "${GITHUB_MATCH}")
        string(REGEX REPLACE "github\\.com[:/]([^/]+)/([^/\\.]+)" "\\2" SHELLPOWER_GITHUB_REPO "${GITHUB_MATCH}")
    endif()
endif()

# Fallback if parsing failed
if(NOT SHELLPOWER_GITHUB_OWNER)
    set(SHELLPOWER_GITHUB_OWNER "unknown")
    set(SHELLPOWER_GITHUB_REPO "shellpower")
endif()

message(STATUS "GitHub repository: ${SHELLPOWER_GITHUB_OWNER}/${SHELLPOWER_GITHUB_REPO}")

# =============================================================================
# Platform Detection
# =============================================================================
if(WIN32)
    set(SHELLPOWER_PLATFORM "windows")
elseif(APPLE)
    set(SHELLPOWER_PLATFORM "macos")
else()
    set(SHELLPOWER_PLATFORM "linux")
endif()

# Detect architecture
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64")
    set(SHELLPOWER_ARCH "arm64")
else()
    set(SHELLPOWER_ARCH "x64")
endif()

message(STATUS "Platform: ${SHELLPOWER_PLATFORM}-${SHELLPOWER_ARCH}")

# Generate version header
configure_file(
    ${CMAKE_SOURCE_DIR}/src/version.h.in
    ${CMAKE_BINARY_DIR}/generated/version.h
    @ONLY
)

# =============================================================================
# Dependencies
# =============================================================================

# Try to find system-installed raylib first (e.g., via Homebrew)
find_package(raylib QUIET)

if(raylib_FOUND)
    message(STATUS "Found system raylib: ${raylib_VERSION}")
else()
    message(STATUS "System raylib not found, fetching via Git...")
    include(FetchContent)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.5
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(raylib)
endif()

# Try to find system-installed libcurl first
find_package(CURL QUIET)

if(CURL_FOUND)
    message(STATUS "Found system curl: ${CURL_VERSION_STRING}")
    set(CURL_TARGET CURL::libcurl)
else()
    message(STATUS "System curl not found, fetching via Git...")
    include(FetchContent)
    FetchContent_Declare(
        curl
        GIT_REPOSITORY https://github.com/curl/curl.git
        GIT_TAG curl-8_5_0
        GIT_SHALLOW TRUE
    )
    # Configure curl build options
    set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(CURL_DISABLE_TESTS ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
    # Use platform SSL
    if(APPLE)
        set(CURL_USE_SECTRANSP ON CACHE BOOL "" FORCE)
    elseif(WIN32)
        set(CURL_USE_SCHANNEL ON CACHE BOOL "" FORCE)
    else()
        set(CURL_USE_OPENSSL ON CACHE BOOL "" FORCE)
    endif()
    FetchContent_MakeAvailable(curl)
    set(CURL_TARGET libcurl)
endif()

# Source files
set(SOURCES
    src/main.c
    src/app.c
    src/auto_layout.c
    src/camera.c
    src/stl_loader.c
    src/gui.c
    src/updater.c
    src/lib/tinyfiledialogs.c
    src/simulation/iv_trace.c
    src/simulation/string_sim.c
)

# Executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/simulation
    ${CMAKE_BINARY_DIR}/generated
)

# Link raylib and curl
target_link_libraries(${PROJECT_NAME} PRIVATE raylib ${CURL_TARGET})

# Platform-specific settings
if(WIN32)
    # tinyfiledialogs requires Comdlg32 and Ole32 on Windows
    target_link_libraries(${PROJECT_NAME} PRIVATE comdlg32 ole32)
elseif(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "-framework OpenGL"
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
    )
elseif(UNIX)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        GL
        m
        pthread
        dl
        rt
        X11
    )
endif()

# Copy assets to build directory (only if assets directory exists)
if(EXISTS ${CMAKE_SOURCE_DIR}/assets)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
    )
endif()

# Compiler warnings (platform-specific)
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
endif()
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
    )
endif()