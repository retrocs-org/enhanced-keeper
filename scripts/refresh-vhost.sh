#!/usr/bin/env bash
# Run on the web server. Keeps config.inc.php contents; applies wwwrun ownership.
set -euo pipefail
kfxRoot="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if (($# == 0)); then
  echo "Usage: $0 /absolute/path/to/keeperfx-e.retrocs.org [--apply] [--initialize]"
  echo 'Default: preview only. Add --apply to refresh; use sudo when changing ownership.'
  exit 2
fi
kfxVhost="$1"
shift
[[ "$kfxVhost" == /* ]] || { echo 'Use the absolute path of the dedicated vhost document root.' >&2; exit 2; }
for kfxOption in "$@"; do
  case "$kfxOption" in --apply|--initialize) ;; *) echo "Unknown option: $kfxOption" >&2; exit 2;; esac
done
exec python3 "$kfxRoot/scripts/deploy-web.py" "$kfxVhost" \
  --owner wwwrun --group wwwrun --set-permissions "$@"
