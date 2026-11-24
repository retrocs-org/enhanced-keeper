# Keeper Enhanced: CMake is the build system; these are convenience targets.
.DEFAULT_GOAL := help
SHELL := /bin/sh
JOBS ?= 2
BUILD_TYPE ?= Release
HOST_OS := $(shell uname -s)
.PHONY: help all server-linux client-linux client-macos client-windows package-server-linux legacy
help:
	@printf '%s\n' 'Keeper Enhanced builds (CMake)' '  make all                 Build native targets (Linux: client + server)' '  make server-linux        Simulation-only Linux server' '  make client-linux        Native Linux client' '  make client-macos        Universal macOS client (run on macOS)' '  make client-windows      Windows client using i686 MinGW-w64' '  make package-server-linux  Build portable Linux server archive' '  make legacy              Original Windows Makefile' 'Options: JOBS=4 BUILD_TYPE=Release'
ifeq ($(HOST_OS),Linux)
all: server-linux client-linux
else ifeq ($(HOST_OS),Darwin)
all: client-macos
else
all: client-windows
endif
server-linux:
	@./scripts/build-target.sh server-linux "$(JOBS)" "$(BUILD_TYPE)"
client-linux:
	@./scripts/build-target.sh client-linux "$(JOBS)" "$(BUILD_TYPE)"
client-macos:
	@./scripts/build-target.sh client-macos "$(JOBS)" "$(BUILD_TYPE)"
client-windows:
	@./scripts/build-target.sh client-windows "$(JOBS)" "$(BUILD_TYPE)"
package-server-linux:
	@KFX_JOBS="$(JOBS)" ./scripts/build-linux-server.sh
legacy:
	+$(MAKE) -f Makefile.legacy all
