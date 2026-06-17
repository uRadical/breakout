# Thin convenience wrapper around CMake. The build itself is defined in
# CMakeLists.txt; these targets just save typing and keep lint/format handy.
#
#   make           configure (if needed) and build
#   make run       build and launch the game
#   make test      build and run the unit tests (CTest)
#   make package   build a distributable (.dmg / .zip / .tar.gz) via CPack
#   make lint      run clang-tidy over the sources
#   make format    rewrite sources to match .clang-format
#   make clean     remove the build directory

BUILD_DIR := build

# On macOS the build produces Breakout.app; elsewhere a plain executable.
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
BIN := $(BUILD_DIR)/Breakout.app/Contents/MacOS/Breakout
else
BIN := $(BUILD_DIR)/breakout
endif

SOURCES := src/Main.cpp src/Game.cpp src/Audio.cpp src/Draw.cpp src/Physics.cpp
HEADERS := include/Game.h include/Audio.h include/Draw.h include/Physics.h
TESTS   := tests/physics_test.cpp

# Homebrew's llvm installs clang-tidy/clang-format but not on PATH.
CLANG_TIDY   := $(shell command -v clang-tidy   || echo /opt/homebrew/opt/llvm/bin/clang-tidy)
CLANG_FORMAT := $(shell command -v clang-format || echo /opt/homebrew/bin/clang-format)

SDL_CFLAGS := $(shell pkg-config --cflags sdl3)

# On macOS, standalone clang-tidy needs the SDK path spelled out explicitly.
ifeq ($(shell uname -s),Darwin)
SYSROOT := -isysroot $(shell xcrun --show-sdk-path)
endif

.PHONY: all configure build run test package lint format format-check clean

all: build

# Configure into BUILD_DIR (only re-runs if the cache is missing).
configure: $(BUILD_DIR)/CMakeCache.txt
$(BUILD_DIR)/CMakeCache.txt:
	cmake -S . -B $(BUILD_DIR)

build: configure
	cmake --build $(BUILD_DIR)

run: build
	./$(BIN)

# Build and run the unit tests via CTest.
test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

# Build a distributable package for the current platform (CPack picks the
# generator: .dmg on macOS, .zip on Windows, .tar.gz on Linux).
package: build
	cd $(BUILD_DIR) && cpack

# Static analysis. Reads checks from .clang-tidy; flags after `--` are the
# compile flags (so no compile_commands.json is required).
lint:
	$(CLANG_TIDY) $(SOURCES) -- -std=c++20 -Iinclude $(SDL_CFLAGS) $(SYSROOT)

# Rewrite sources in place to match .clang-format.
format:
	$(CLANG_FORMAT) -i $(SOURCES) $(HEADERS) $(TESTS)

# Fail (without editing) if anything isn't formatted -- handy for CI.
format-check:
	$(CLANG_FORMAT) --dry-run --Werror $(SOURCES) $(HEADERS) $(TESTS)

clean:
	rm -rf $(BUILD_DIR)
