#!/bin/sh
# Refresh the PHP vhost from GitHub main, preserving the live config.inc.php.
set -eu
APP=keeperfx-e
PARENT=/srv/www/vhosts
SITE="$PARENT/$APP.retrocs.org"
REPOSITORY=https://github.com/retrocs-org/KeeperFX-E.git

[ "$(id -u)" -eq 0 ] || { echo 'Run as root.' >&2; exit 1; }
for program in git php getent flock; do
    command -v "$program" >/dev/null 2>&1 || { echo "Missing command: $program" >&2; exit 1; }
done
[ ! -L "$SITE" ] || { echo 'Refusing a symlink document root.' >&2; exit 1; }
[ ! -e "$SITE" ] || [ -d "$SITE" ] || { echo 'Document root is not a directory.' >&2; exit 1; }
getent passwd wwwrun >/dev/null
getent group wwwrun >/dev/null
mkdir -p "$PARENT"
# Hold the same lock inode for the complete update; concurrent refreshes are refused.
LOCK="$PARENT/.refresh-$APP.lock"
[ ! -L "$LOCK" ] || { echo 'Refusing a symlink lock file.' >&2; exit 1; }
exec 9>"$LOCK"
flock -n 9 || { echo 'Another website refresh is running.' >&2; exit 1; }
umask 077
TEMP=$(mktemp -d "$PARENT/.refresh-$APP.XXXXXX")
cleanup() {
    result=$?
    trap - 0 HUP INT TERM
    if [ -d "$TEMP/previous" ] && [ ! -e "$SITE" ] && [ ! -L "$SITE" ]; then
        if ! mv "$TEMP/previous" "$SITE"; then
            echo "Restore failed. Previous site retained at $TEMP/previous" >&2
            exit 1
        fi
    fi
    rm -rf -- "$TEMP"
    exit "$result"
}
trap cleanup 0
trap 'exit 1' HUP INT TERM

git clone --quiet --depth 1 --single-branch --branch main \
    "$REPOSITORY" "$TEMP/repository"
SOURCE="$TEMP/repository/public-web"
[ -f "$SOURCE/index.php" ] && [ -f "$SOURCE/config.inc.php" ] || {
    echo 'Website index.php/config.inc.php missing; site left unchanged.' >&2
    exit 1
}
[ -z "$(find "$SOURCE" -type l -print -quit)" ] || {
    echo 'Website source contains symlinks; site left unchanged.' >&2
    exit 1
}
mkdir "$TEMP/new"
cp -R "$SOURCE/." "$TEMP/new/"

# Always prefer the live server settings. Never merge in the repository template.
if [ -L "$SITE/config.inc.php" ] || { [ -e "$SITE/config.inc.php" ] && [ ! -f "$SITE/config.inc.php" ]; }; then
    echo 'Existing config.inc.php is not a regular file; site left unchanged.' >&2
    exit 1
elif [ -f "$SITE/config.inc.php" ]; then
    cp -p "$SITE/config.inc.php" "$TEMP/new/config.inc.php"
    echo 'Preserving existing config.inc.php contents.'
else
    echo 'Installing sample config.inc.php; configure it before enabling services.'
fi

# Validate the FINAL tree, including the preserved config. Suppress PHP diagnostics
# because a syntax error could otherwise quote part of a private configuration.
find "$TEMP/new" -type f -name '*.php' -exec sh -c '
    marker=$1
    shift
    for file do
        php -l "$file" >/dev/null 2>&1 || { touch "$marker"; exit 1; }
    done
' sh "$TEMP/lint-failed" {} +
[ ! -e "$TEMP/lint-failed" ] || {
    echo 'PHP lint failed (including server config); site left unchanged.' >&2
    exit 1
}
chown -R wwwrun:wwwrun "$TEMP/new"
find "$TEMP/new" -type d -exec chmod 750 {} +
find "$TEMP/new" -type f -exec chmod 640 {} +
# Keep compatibility with the checkout-based refresh tool.
printf 'KeeperFX-E public-web v1\n' > "$TEMP/new/.keeperfx-e-vhost"
chown wwwrun:wwwrun "$TEMP/new/.keeperfx-e-vhost"
chmod 640 "$TEMP/new/.keeperfx-e-vhost"

# Stage on the same filesystem. The previous tree remains available for rollback
# until the new tree is in place. No database migration or service restart runs.
[ ! -L "$SITE" ] || { echo 'Document root became a symlink; refusing update.' >&2; exit 1; }
if [ -e "$SITE" ]; then mv "$SITE" "$TEMP/previous"; fi
mv "$TEMP/new" "$SITE"
echo "Updated $SITE from GitHub main (wwwrun:wwwrun; directories 0750, files 0640)."
