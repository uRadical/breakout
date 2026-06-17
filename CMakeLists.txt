cmake_minimum_required(VERSION 3.21)

project(breakout
    VERSION 1.0.0
    DESCRIPTION "Breakout, a small arcade game built on SDL3"
    HOMEPAGE_URL "https://uradical.io"
    LANGUAGES CXX)

# --- Standard ---------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Always emit compile_commands.json (consumed by clangd / clang-tidy).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to a Debug build if the user didn't pick one.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# --- Dependencies -----------------------------------------------------------
# Prefer a system SDL3 (Homebrew ships SDL3Config.cmake; CMAKE_PREFIX_PATH may
# need /opt/homebrew). If none is found -- e.g. on CI runners -- fetch and build
# SDL3 from source so the build is self-contained on every platform.
find_package(SDL3 QUIET CONFIG)
if(NOT SDL3_FOUND)
    message(STATUS "System SDL3 not found -- fetching SDL3 from source")
    include(FetchContent)
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.4.10
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(SDL3)
endif()

# --- Platform-specific icon resources --------------------------------------
set(ICON_SOURCES "")
if(WIN32)
    list(APPEND ICON_SOURCES assets/breakout.rc) # embeds assets/icon.ico
elseif(APPLE)
    list(APPEND ICON_SOURCES assets/icon.icns)   # copied into the .app below
endif()

# --- Targets ----------------------------------------------------------------
# Pure game maths, isolated (no SDL) so it can be unit-tested on its own.
add_library(breakout_physics STATIC src/Physics.cpp)
target_include_directories(breakout_physics PUBLIC include)

add_executable(breakout
    src/Main.cpp
    src/Game.cpp
    src/Audio.cpp
    src/Draw.cpp
    ${ICON_SOURCES})

target_include_directories(breakout PRIVATE include)
target_link_libraries(breakout PRIVATE SDL3::SDL3 breakout_physics)

# SDL3's entry-point shim (SDL_main.h). The SDL3::SDL3main target exists when SDL
# is built from source (CI); a system SDL3 (e.g. Homebrew) may omit it, in which
# case the header is self-contained.
if(TARGET SDL3::SDL3main)
    target_link_libraries(breakout PRIVATE SDL3::SDL3main)
endif()

if(MSVC)
    target_compile_options(breakout PRIVATE /W4)
else()
    target_compile_options(breakout PRIVATE -Wall -Wextra)
endif()

# --- Tests ------------------------------------------------------------------
option(BREAKOUT_BUILD_TESTS "Build the unit tests" ON)
if(BREAKOUT_BUILD_TESTS)
    enable_testing()
    add_executable(physics_test tests/physics_test.cpp)
    target_link_libraries(physics_test PRIVATE breakout_physics)
    add_test(NAME physics COMMAND physics_test)
endif()

# --- Per-platform app packaging --------------------------------------------
if(APPLE)
    # Build a Breakout.app bundle with the icon and Info.plist metadata.
    set_source_files_properties(assets/icon.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
    set_target_properties(breakout PROPERTIES
        MACOSX_BUNDLE TRUE
        OUTPUT_NAME "Breakout"
        MACOSX_BUNDLE_BUNDLE_NAME "Breakout"
        MACOSX_BUNDLE_GUI_IDENTIFIER "io.uradical.breakout"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_ICON_FILE "icon.icns"
        MACOSX_BUNDLE_COPYRIGHT "(c) uRadical")
elseif(WIN32)
    # GUI subsystem: no console window alongside the game.
    set_target_properties(breakout PROPERTIES WIN32_EXECUTABLE TRUE)
    # For local runs, drop SDL3.dll (and friends) next to the .exe.
    add_custom_command(TARGET breakout POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:breakout> $<TARGET_FILE_DIR:breakout>
        COMMAND_EXPAND_LISTS)
endif()

# --- Install rules (drive both `cmake --install` and CPack) -----------------
if(APPLE)
    install(TARGETS breakout BUNDLE DESTINATION .)
    # Copy SDL3 into the bundle and rewrite load paths so Breakout.app runs on
    # any Mac without Homebrew. Written via file(GENERATE) so the SDL3 location
    # ($<...>) is resolved now while ${CMAKE_INSTALL_PREFIX} stays deferred to
    # install time (CPack overrides it with its staging dir).
    file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/fixup_bundle.cmake" CONTENT [=[
        include(BundleUtilities)
        set(app "${CMAKE_INSTALL_PREFIX}/Breakout.app")
        fixup_bundle("${app}" "" "$<TARGET_FILE_DIR:SDL3::SDL3>")
        # Re-sign ad-hoc: fixup_bundle's edits invalidate the signature, which
        # would otherwise block launch on Apple Silicon.
        execute_process(COMMAND codesign --force --deep --sign - "${app}")
    ]=])
    install(SCRIPT "${CMAKE_BINARY_DIR}/fixup_bundle.cmake")
elseif(WIN32)
    install(TARGETS breakout RUNTIME DESTINATION .)
    install(FILES $<TARGET_RUNTIME_DLLS:breakout> DESTINATION . OPTIONAL)
else() # Linux / other Unix
    # Look for SDL3 next to the binary (../lib) so a bundled .so / AppImage
    # layout resolves without a system install.
    set_target_properties(breakout PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
    install(TARGETS breakout RUNTIME DESTINATION bin)
    install(FILES assets/breakout.desktop DESTINATION share/applications)
    # 512x512 PNG: linuxdeploy requires the icon's pixel size to match the
    # hicolor directory (the 1024x1024 master would be rejected).
    install(FILES assets/icon_512.png DESTINATION share/icons/hicolor/512x512/apps
        RENAME breakout.png)
endif()

# --- CPack: `cpack` (or `make package`) builds the distributable ------------
set(CPACK_PACKAGE_NAME "Breakout")
set(CPACK_PACKAGE_VENDOR "uRadical")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "Breakout-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}")
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")
if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")    # -> .dmg
elseif(WIN32)
    set(CPACK_GENERATOR "ZIP")          # -> .zip
else()
    set(CPACK_GENERATOR "TGZ")          # -> .tar.gz
endif()
include(CPack)
