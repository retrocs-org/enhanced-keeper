# Keeper Enhanced: CMake is the build system; these are convenience targets.
.DEFAULT_GOAL := help
SHELL := /bin/sh
JOBS ?= 2
BUILD_TYPE ?= Release
OFFLINE ?= 0
IMPORT_BASE ?= $(if $(wildcard $(CURDIR)/assets/runtime/keeper-enhanced.cfg),$(CURDIR)/assets/runtime,$(CURDIR)/.local/generated-runtime)
SERVER_DATA_BASE ?= $(CURDIR)/.local/imported-client
IMPORT_DIR ?= $(CURDIR)/import
IMAGE ?= $(KFX_IMPORT_IMAGE)
IMPORT_OUTPUT ?=
HOST_OS := $(shell uname -s)
.PHONY: help all server-linux client-linux client-macos client-windows package-server-linux legacy import import-client import-server server-data clean clean-preview tools
help:
	@printf '%s\n' 'Keeper Enhanced builds (CMake)' '  make all                 Build native targets (Linux: client + server)' '  make server-linux        Stage minimal imported data + build Linux server' '  make server-data         Refresh generated minimal data only' '  make client-linux        Native Linux client' '  make client-macos        Universal macOS client (run on macOS)' '  make client-windows      Windows client using i686 MinGW-w64' '  make package-server-linux  Build portable Linux server archive' '  make import              Import client data and CD music' '  make import-server       Import minimal server data (no music/movies)' '  make tools               Build native tools under tools/local/' '  make clean               Remove builds, imported data and local tools' '  make clean-preview       List cleanup paths without deleting' '  make legacy              Original Windows Makefile' 'Import options: IMPORT_BASE=path IMAGE=disc.cue IMPORT_OUTPUT=path' 'Options: JOBS=4 BUILD_TYPE=Release'
ifeq ($(HOST_OS),Linux)
all: server-linux client-linux
else ifeq ($(HOST_OS),Darwin)
all: client-macos
else
all: client-windows
endif
server-linux: server-data
	@./scripts/build-target.sh server-linux "$(JOBS)" "$(BUILD_TYPE)"
client-linux:
	@./scripts/build-target.sh client-linux "$(JOBS)" "$(BUILD_TYPE)"
client-macos:
	@./scripts/build-target.sh client-macos "$(JOBS)" "$(BUILD_TYPE)"
client-windows:
	@./scripts/build-target.sh client-windows "$(JOBS)" "$(BUILD_TYPE)"
package-server-linux: server-data
	@KFX_JOBS="$(JOBS)" ./scripts/build-linux-server.sh
legacy:
	+$(MAKE) -f Makefile.legacy all

# These prepare local data only; no upload, service restart or overwrite.
import: import-client
import-client:
	@KFX_JOBS="$(JOBS)" KFX_ASSET_OFFLINE="$(OFFLINE)" python3 scripts/import-data.py client --base "$(IMPORT_BASE)" --import-dir "$(IMPORT_DIR)" --image "$(IMAGE)" --output "$(IMPORT_OUTPUT)"
import-server:
	@KFX_JOBS="$(JOBS)" KFX_ASSET_OFFLINE="$(OFFLINE)" python3 scripts/import-data.py server --base "$(IMPORT_BASE)" --import-dir "$(IMPORT_DIR)" --image "$(IMAGE)" --output "$(IMPORT_OUTPUT)"

server-data:
	@python3 scripts/stage-server-data.py --source "$(SERVER_DATA_BASE)" --output "$(CURDIR)/out/server-linux-data"

clean:
	@python3 scripts/clean.py
clean-preview:
	@python3 scripts/clean.py --dry-run

tools:
	@python3 scripts/build-asset-runtime.py --tools-only --jobs "$(JOBS)" $(if $(filter 1,$(OFFLINE)),--offline,)
