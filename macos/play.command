#!/bin/zsh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GAME="${KFX_GAME_DIR:-$ROOT/.local/keeperfx}"
if [[ ! -x "$GAME/keeper-enhanced" || ! -f "$GAME/keeper-enhanced.cfg" ]]; then
  echo "KeeperFX runtime is missing from $GAME" >&2
  exit 1
fi
exec "$GAME/keeper-enhanced" "$@"
