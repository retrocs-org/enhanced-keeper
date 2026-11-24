#!/usr/bin/env bash
# Refresh an existing native Linux dedicated-server installation from GitHub main.
set -Eeuo pipefail
umask 077
SOURCE=${KFX_SERVER_SOURCE:-/root/keeper-enhanced}
REPOSITORY=https://github.com/retrocs-org/KeeperFX-E.git
# Same CMake build directory as make server-linux; old caches remain untouched.
BUILD="$SOURCE/out/server-linux"
DEST=/opt/keeper-enhanced-server
CONFIG=/etc/keeper-enhanced/server.json
STATE=/var/lib/keeper-enhanced
SERVICE=keeper-enhanced-server.service
[[ $(id -u) == 0 && $(uname -s) == Linux ]] || { echo 'Run as root on Linux.' >&2; exit 1; }
for tool in git cmake make pkg-config systemctl install flock nproc mktemp python3 runuser ldd patchelf; do
    command -v "$tool" >/dev/null || { echo "Missing command: $tool" >&2; exit 1; }
done
JOBS=${KFX_JOBS:-$(nproc)}
case "$JOBS" in ''|*[!0-9]*|0) echo 'KFX_JOBS must be a positive integer.' >&2; exit 1;; esac
python3 -c 'import sys; assert sys.version_info >= (3,9), "Python 3.9+ required"'
pkg-config --exists libcurl luajit minizip zlib openssl || {
    echo 'Missing server libraries. Install providers of pkgconfig(libcurl), pkgconfig(luajit), pkgconfig(minizip), pkgconfig(zlib), pkgconfig(openssl).' >&2; exit 1;
}
exec 9>/run/lock/keeper-enhanced-server-refresh.lock
flock -n 9 || { echo 'Another server refresh is running.' >&2; exit 1; }
# Always obtain source from the public project, independent of local origin settings.
[[ ! -L "$SOURCE" ]] || { echo 'Source checkout must not be a symlink.' >&2; exit 1; }
if [[ ! -e "$SOURCE" ]]; then
    git clone --single-branch --branch main "$REPOSITORY" "$SOURCE"
fi
[[ -d "$SOURCE/.git" || -f "$SOURCE/.git" ]] || { echo "$SOURCE is not a Git checkout; nothing overwritten." >&2; exit 1; }
[[ $(git -C "$SOURCE" branch --show-current) == main ]] || { echo "$SOURCE must be on main." >&2; exit 1; }
git -C "$SOURCE" diff --quiet && git -C "$SOURCE" diff --cached --quiet || { echo 'Commit or stash tracked source changes first.' >&2; exit 1; }
echo 'Fetching latest GitHub main; the current server stays running.'
git -C "$SOURCE" fetch --no-tags "$REPOSITORY" refs/heads/main
LATEST=$(git -C "$SOURCE" rev-parse FETCH_HEAD)
git -C "$SOURCE" merge --ff-only "$LATEST"
[[ $(git -C "$SOURCE" rev-parse HEAD) == "$LATEST" ]] || {
    echo 'Local main has unpublished commits; refusing to deploy code that differs from GitHub.' >&2; exit 1;
}
echo "Building GitHub revision: $LATEST"
[[ -d "$DEST" && ! -L "$DEST" && ( -x "$DEST/engine/keeper-enhanced" || -x "$DEST/engine/keeperfx" ) && -f "$CONFIG" && -d "$STATE" && ! -L "$STATE" && ! -L "$STATE/dedicated" ]] || {
    echo 'Refresh requires an installed dedicated server; it does not perform first-time setup.' >&2
    echo 'For a first build, run make package-server-linux in the source checkout, then follow docs/LINUX-SERVER.md.' >&2
    echo "Expected engine: $DEST/engine/keeper-enhanced (legacy keeperfx also accepted)" >&2
    echo "Expected configuration: $CONFIG; state directory: $STATE" >&2
    exit 1;
}
UNIT=$(systemctl show "$SERVICE" -p FragmentPath --value)
case "$UNIT" in
    /etc/systemd/system/keeper-enhanced-server.service|/usr/lib/systemd/system/keeper-enhanced-server.service|/lib/systemd/system/keeper-enhanced-server.service) ;;
    *) echo "Unexpected service location: $UNIT" >&2; exit 1 ;;
esac
[[ -f "$UNIT" && ! -L "$UNIT" && ! -L "$CONFIG" ]] || { echo 'Unit and config must be regular files.' >&2; exit 1; }
[[ $(systemctl show "$SERVICE" -p User --value) == keeper-enhanced ]] || { echo 'Expected service user keeper-enhanced.' >&2; exit 1; }
# This refresh targets the standard install paths. Never silently replace custom deployments.
python3 - "$CONFIG" "$DEST" "$STATE" <<'PY'
import json,sys
from pathlib import Path
c=json.load(open(sys.argv[1]))
if Path(c['engine_binary']) not in (Path(sys.argv[2])/'engine/keeper-enhanced', Path(sys.argv[2])/'engine/keeperfx') or Path(c['user_data']) != Path(sys.argv[3]):
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

echo 'Updating source and building the dedicated Linux server; the current server stays running.'
cmake -S "$SOURCE" -B "$BUILD" -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release \
    -DKFX_DEDICATED=ON -DKFX_DECK_BUNDLE=ON -DKFX_INSTALL_GAME_DATA=OFF
cmake --build "$BUILD" --config Release --target keeper-enhanced --parallel "$JOBS"
cmake --install "$BUILD" --prefix "$STAGED/new/engine"
"$STAGED/new/engine/keeper-enhanced" --version
# Compatibility for existing server.json files pointing at the old executable.
ln -s keeper-enhanced "$STAGED/new/engine/keeperfx"
cp "$SOURCE/services/keeperhub/dedicated.py" "$STAGED/new/dedicated.py"
cp "$SOURCE/linux/import-data.py" "$SOURCE/linux/server.json" "$SOURCE/linux/keeper-enhanced-server.service" "$STAGED/new/"
cp "$SOURCE/docs/LINUX-SERVER.md" "$STAGED/new/README.md"
cp "$SOURCE/LICENSE" "$STAGED/new/"
# Public binaries/scripts only; credentials and user data are never staged here.
chmod -R a+rX "$STAGED"
ldd "$STAGED/new/engine/keeper-enhanced" > "$STAGED/libraries.log"
if grep -q 'not found' "$STAGED/libraries.log"; then
    cat "$STAGED/libraries.log" >&2
    echo 'Runtime libraries missing; current server left unchanged.' >&2; exit 1
fi
if grep -Eiq 'lib(SDL[23](_image|_mixer)?|openal|avcodec|avformat|avutil|swscale|swresample|GL|EGL|GLX|vulkan|X11|wayland|asound|pulse)' "$STAGED/libraries.log"; then
    echo 'Unexpected presentation dependency in dedicated binary; current server left unchanged.' >&2; exit 1
fi
# Validate with the new supervisor and staged executable as the service user.
# Do not source server.env: it uses systemd syntax and may contain secrets.
runuser -u keeper-enhanced -- python3 - "$STAGED/new" "$CONFIG" <<'PY'
import importlib.util,json,sys
from pathlib import Path
stage=Path(sys.argv[1]);spec=importlib.util.spec_from_file_location('dedicated',stage/'dedicated.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
c=json.load(open(sys.argv[2]));c['engine_binary']=str(stage/'engine/keeper-enhanced');m.validate(c)
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
