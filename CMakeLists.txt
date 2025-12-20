cmake_minimum_required(VERSION 3.14)
project(shellpower C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

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

# Source files
set(SOURCES
    src/main.c
    src/app.c
    src/gui.c
    src/lib/tinyfiledialogs.c
)

# Executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link raylib
target_link_libraries(${PROJECT_NAME} PRIVATE raylib)

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