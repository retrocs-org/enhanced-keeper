#!/usr/bin/env bash
# Refresh an existing native Linux dedicated-server installation from GitHub main.
set -Eeuo pipefail
umask 077
SOURCE=${KFX_SERVER_SOURCE:-/root/keeper-enhanced}
# Separate directory avoids conflicts with earlier Ninja-generated caches.
BUILD="$SOURCE/build-server-make"
DEST=/opt/keeper-enhanced-server
CONFIG=/etc/keeper-enhanced/server.json
STATE=/var/lib/keeper-enhanced
SERVICE=keeper-enhanced-server.service
[[ $(id -u) == 0 && $(uname -s) == Linux ]] || { echo 'Run as root on Linux.' >&2; exit 1; }
for tool in git cmake make pkg-config systemctl install flock nproc mktemp python3 runuser ldd; do
    command -v "$tool" >/dev/null || { echo "Missing command: $tool" >&2; exit 1; }
done
python3 -c 'import sys; assert sys.version_info >= (3,9), "Python 3.9+ required"'
pkg-config --exists libavformat libavcodec libswresample libswscale libavutil openal luajit spng minizip zlib || {
    echo 'Missing development libraries. See docs/LINUX-SERVER.md and the CMake dependency report.' >&2; exit 1;
}
exec 9>/run/lock/keeper-enhanced-server-refresh.lock
flock -n 9 || { echo 'Another server refresh is running.' >&2; exit 1; }
[[ -d "$DEST" && ! -L "$DEST" && -x "$DEST/engine/keeperfx" && -f "$CONFIG" && -d "$STATE" && ! -L "$STATE" && ! -L "$STATE/dedicated" ]] || {
    echo 'An existing dedicated-server installation and imported game data are required.' >&2; exit 1;
}
UNIT=$(systemctl show "$SERVICE" -p FragmentPath --value)
case "$UNIT" in
    /etc/systemd/system/keeper-enhanced-server.service|/usr/lib/systemd/system/keeper-enhanced-server.service|/lib/systemd/system/keeper-enhanced-server.service) ;;
    *) echo "Unexpected service location: $UNIT" >&2; exit 1 ;;
esac
[[ -f "$UNIT" && ! -L "$UNIT" && ! -L "$CONFIG" ]] || { echo 'Unit and config must be regular files.' >&2; exit 1; }
[[ $(systemctl show "$SERVICE" -p User --value) == keeper-enhanced ]] || { echo 'Expected service user keeper-enhanced.' >&2; exit 1; }
[[ $(git -C "$SOURCE" branch --show-current) == main ]] || { echo "$SOURCE must be on main." >&2; exit 1; }
git -C "$SOURCE" diff --quiet && git -C "$SOURCE" diff --cached --quiet || { echo 'Commit or stash tracked source changes first.' >&2; exit 1; }
# This refresh targets the standard install paths. Never silently replace custom deployments.
python3 - "$CONFIG" "$DEST" "$STATE" <<'PY'
import json,sys
from pathlib import Path
c=json.load(open(sys.argv[1]))
if Path(c['engine_binary']) != Path(sys.argv[2])/'engine/keeperfx' or Path(c['user_data']) != Path(sys.argv[3]):
    raise SystemExit('Custom engine/profile paths: adapt this refresh script before use.')
PY
WAS_ACTIVE=0
systemctl is-active --quiet "$SERVICE" && WAS_ACTIVE=1
STAGED=$(mktemp -d /opt/.keeper-enhanced-refresh.XXXXXXXX)
BACKUP=''
INSTALLING=0
finish() {
    result=$?
    trap - EXIT
    if (( result != 0 && INSTALLING == 1 )); then
        echo "Refresh failed; restoring the previous server. Backup: $BACKUP" >&2
        set +e
        systemctl stop "$SERVICE"
        # Restore the complete old bundle with a same-filesystem rename.
        if [[ -d "$STAGED/old" ]]; then
            if [[ -e "$DEST" ]] && ! mv "$DEST" "$STAGED/failed-install"; then
                echo "Rollback blocked. Previous bundle retained at $STAGED/old and $BACKUP/previous." >&2
                exit "$result"
            fi
            if ! mv "$STAGED/old" "$DEST"; then
                echo "Rollback failed. Previous bundle retained at $STAGED/old and $BACKUP/previous." >&2
                exit "$result"
            fi
        fi
        # The existing unit, configuration and environment file were never replaced.
        if (( WAS_ACTIVE )); then systemctl start "$SERVICE" || echo "Previous server failed to start; inspect journalctl -u $SERVICE." >&2; fi
    fi
    rm -rf -- "$STAGED"
    exit "$result"
}
trap finish EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

echo 'Updating and building; the current server stays running.'
git -C "$SOURCE" pull --ff-only origin main
cmake -S "$SOURCE" -B "$BUILD" -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release \
    -DKFX_DECK_BUNDLE=ON -DKFX_INSTALL_GAME_DATA=OFF
cmake --build "$BUILD" --target keeperfx --parallel "${KFX_JOBS:-$(nproc)}"
cmake --install "$BUILD" --prefix "$STAGED/new/engine"
cp "$SOURCE/services/keeperhub/dedicated.py" "$STAGED/new/dedicated.py"
cp "$SOURCE/linux/import-data.py" "$SOURCE/linux/server.json" "$SOURCE/linux/keeper-enhanced-server.service" "$STAGED/new/"
cp "$SOURCE/docs/LINUX-SERVER.md" "$STAGED/new/README.md"
cp "$SOURCE/LICENSE" "$STAGED/new/"
# Public binaries/scripts only; credentials and user data are never staged here.
chmod -R a+rX "$STAGED"
ldd "$STAGED/new/engine/keeperfx" > "$STAGED/libraries.log"
if grep -q 'not found' "$STAGED/libraries.log"; then
    cat "$STAGED/libraries.log" >&2
    echo 'Runtime libraries missing; current server left unchanged.' >&2; exit 1
fi
# Validate with the new supervisor and staged executable as the service user.
# Do not source server.env: it uses systemd syntax and may contain secrets.
runuser -u keeper-enhanced -- python3 - "$STAGED/new" "$CONFIG" <<'PY'
import importlib.util,json,sys
from pathlib import Path
stage=Path(sys.argv[1]);spec=importlib.util.spec_from_file_location('dedicated',stage/'dedicated.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
c=json.load(open(sys.argv[2]));c['engine_binary']=str(stage/'engine/keeperfx');m.validate(c)
print('New supervisor and existing configuration validated.')
PY
install -d -m 700 /var/backups/keeper-enhanced-server
BACKUP=$(mktemp -d /var/backups/keeper-enhanced-server/refresh-XXXXXXXX)
cp -a "$DEST" "$BACKUP/previous"
cp -p "$UNIT" "$BACKUP/server.service"
cp -p "$CONFIG" "$BACKUP/server.json"
if [[ -f /etc/keeper-enhanced/server.env ]]; then cp -p /etc/keeper-enhanced/server.env "$BACKUP/server.env"; fi
# Keep the old unit (including its local tuning); only engine/supervisor bytes change.
INSTALLING=1
systemctl stop "$SERVICE"
rm -f "$STATE/dedicated/status.json"
# Same-filesystem swap; the backup above also survives a later restart failure.
mv "$DEST" "$STAGED/old"
mv "$STAGED/new" "$DEST"
systemctl start "$SERVICE"
# systemd may report active before the engine is ready. Require a fresh RPC status
# and a stable supervisor PID, rather than treating a restart loop as success.
python3 - "$SERVICE" "$STATE/dedicated/status.json" <<'PY'
import json,subprocess,sys,time
from pathlib import Path
service,path=sys.argv[1],Path(sys.argv[2]);deadline=time.monotonic()+60;pid=None;stable=0
while time.monotonic()<deadline:
    props=subprocess.check_output(['systemctl','show',service,'-p','ActiveState','-p','MainPID'],text=True)
    values=dict(line.split('=',1) for line in props.splitlines());current=values.get('MainPID','0')
    try:ready=json.loads(path.read_text()).get('active') is True and time.time()-path.stat().st_mtime<5
    except (OSError,ValueError):ready=False
    if values.get('ActiveState')=='active' and current!='0' and current==pid and ready:stable+=1
    else:stable=0
    pid=current
    if stable>=3:print('Dedicated engine is responding.');break
    time.sleep(1)
else:raise SystemExit('Server did not become ready within 60 seconds; rolling back.')
PY
INSTALLING=0
echo "Server refreshed. Config, secrets, game data and saves preserved. Backup: $BACKUP"
systemctl --no-pager --full status "$SERVICE"
