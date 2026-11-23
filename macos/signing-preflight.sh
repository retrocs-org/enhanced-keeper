#!/bin/zsh
set -euo pipefail
identity="${1:?Supply the configured Developer ID identity}"
folder="$(mktemp -d "${TMPDIR:-/tmp}/keeperfx-signing.XXXXXX")"
trap 'rm -rf "$folder"' EXIT
printf 'int main(void){return 0;}\n' > "$folder/probe.c"
clang "$folder/probe.c" -o "$folder/probe"
if ! codesign --force --options runtime --timestamp --sign "$identity" "$folder/probe" > "$folder/result" 2>&1; then
  echo 'Signing preflight failed. Unlock the keychain containing your Developer ID private key and allow codesign access in Keychain Access, then retry. No package was created.' >&2
  exit 1
fi
codesign --verify --strict "$folder/probe"
echo 'Developer ID signing preflight passed.'
