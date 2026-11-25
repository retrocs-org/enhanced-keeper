#!/bin/zsh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
THIRD="$ROOT/third_party"
mkdir -p "$THIRD"

clone_at() {
  local name="$1" url="$2" rev="$3"
  local dst="$THIRD/$name"
  if [[ ! -d "$dst/.git" ]]; then
    git clone "$url" "$dst"
  fi
  git -C "$dst" fetch --tags --quiet
  git -C "$dst" checkout --detach "$rev"
}

# Keep upstream build-only vendor sources outside the source-only repository.
clone_at keeperfx-source https://github.com/lurcheous73/keeperfx-macos.git bb65dafd7d430a80fb22a426abf6a1c7354fb441
clone_at centijson https://github.com/mity/centijson.git 8c7a5fb
clone_at astronomy https://github.com/cosinekitty/astronomy.git 865d3da
clone_at enet6 https://github.com/SirLynix/enet6.git bf0003f
for dependency in astronomy enet6; do
  PATCH="$ROOT/macos/$dependency-compat.patch"
  if [[ -s "$PATCH" ]]; then
    if git -C "$THIRD/$dependency" apply --check "$PATCH" >/dev/null 2>&1; then
      git -C "$THIRD/$dependency" apply "$PATCH"
    elif git -C "$THIRD/$dependency" apply --reverse --check "$PATCH" >/dev/null 2>&1; then
      echo "$dependency compatibility patch already applied"
    else
      echo "Cannot apply $dependency compatibility patch" >&2
      exit 1
    fi
  fi
done

echo "Pinned macOS source dependencies are ready in $THIRD"
