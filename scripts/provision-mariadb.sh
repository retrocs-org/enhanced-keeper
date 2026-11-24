#!/usr/bin/env bash
# Run on the database server as the owner of the administrative .my.cnf.
set +x
set -euo pipefail
umask 077
kfxRepoRoot="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
kfxDbName=keeperfx_e
kfxDbUser=keeperfx_e
kfxWebHost=172.16.1.133
kfxDbHost=172.16.1.121
kfxDefaults="$HOME/.my.cnf"
kfxOutput="$HOME/keeperfx-e.database.php"
kfxSchema="$kfxRepoRoot/public-web/lib/mariadb.sql"
while (($#)); do
  case "$1" in
    --database) kfxDbName="${2:?}"; shift 2;;
    --user) kfxDbUser="${2:?}"; shift 2;;
    --web-host) kfxWebHost="${2:?}"; shift 2;;
    --database-host) kfxDbHost="${2:?}"; shift 2;;
    --defaults-file) kfxDefaults="${2:?}"; shift 2;;
    --output) kfxOutput="${2:?}"; shift 2;;
    --schema) kfxSchema="${2:?}"; shift 2;;
    *) echo "Unknown option: $1" >&2; exit 2;;
  esac
done
[[ "$kfxDbName" =~ ^[A-Za-z][A-Za-z0-9_]{0,31}$ && "$kfxDbUser" =~ ^[A-Za-z][A-Za-z0-9_]{0,31}$ ]] || { echo 'Use simple database/account names.' >&2; exit 2; }
[[ "$kfxWebHost" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ && "$kfxDbHost" =~ ^[A-Za-z0-9][A-Za-z0-9.-]+$ ]] || { echo 'Use the specific web server IPv4 address and database hostname/IP.' >&2; exit 2; }
[[ -f "$kfxDefaults" && ! -L "$kfxDefaults" && -f "$kfxSchema" ]] || { echo 'Missing regular administrative defaults file or schema.' >&2; exit 2; }
[[ ! -e "$kfxOutput" && ! -L "$kfxOutput" ]] || { echo 'Credential output already exists; refusing to overwrite or rotate an existing account.' >&2; exit 2; }
kfxClient="${MARIADB_CLIENT:-mariadb}"
kfxExists="$("$kfxClient" --defaults-extra-file="$kfxDefaults" --batch --skip-column-names -e "SELECT COUNT(*) FROM mysql.user WHERE User='$kfxDbUser' AND Host='$kfxWebHost'")"
[[ "$kfxExists" == 0 ]] || { echo "Account $kfxDbUser@$kfxWebHost already exists; no password was changed." >&2; exit 2; }
kfxPassword="$(openssl rand -hex 32)"
kfxCreatedUser=0
kfxOutputOwned=0
cleanup() {
  kfxStatus=$?
  if ((kfxStatus != 0 && kfxCreatedUser)); then
    printf "DROP USER IF EXISTS '%s'@'%s';\n" "$kfxDbUser" "$kfxWebHost" | "$kfxClient" --defaults-extra-file="$kfxDefaults" --batch >/dev/null 2>&1 || true
    if ((kfxOutputOwned)); then rm -f -- "$kfxOutput"; fi
  fi
  unset kfxPassword
}
trap cleanup EXIT
printf 'CREATE DATABASE IF NOT EXISTS `%s` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;\n' "$kfxDbName" | "$kfxClient" --defaults-extra-file="$kfxDefaults" --batch
if ! printf "CREATE USER '%s'@'%s' IDENTIFIED BY '%s';\n" "$kfxDbUser" "$kfxWebHost" "$kfxPassword" | "$kfxClient" --defaults-extra-file="$kfxDefaults" --batch 2>/dev/null; then
  echo "Could not create the application account; no credentials were printed." >&2; exit 1
fi
kfxCreatedUser=1
"$kfxClient" --defaults-extra-file="$kfxDefaults" --default-character-set=utf8mb4 "$kfxDbName" < "$kfxSchema"
printf 'GRANT SELECT,INSERT,UPDATE,DELETE ON `%s`.* TO '\''%s'\''@'\''%s'\'';\n' "$kfxDbName" "$kfxDbUser" "$kfxWebHost" | "$kfxClient" --defaults-extra-file="$kfxDefaults" --batch
set -o noclobber
exec 3> "$kfxOutput"
kfxOutputOwned=1
printf "<?php\n// Private KeeperFX-E database settings. Keep outside the web document root.\nreturn [\n 'dsn'=>'mysql:host=%s;port=3306;dbname=%s;charset=utf8mb4',\n 'user'=>'%s',\n 'password'=>'%s',\n];\n" "$kfxDbHost" "$kfxDbName" "$kfxDbUser" "$kfxPassword" >&3
exec 3>&-
echo "Created database $kfxDbName and account $kfxDbUser@$kfxWebHost with application-only CRUD permissions."
echo "Private PHP database configuration: $kfxOutput"
echo 'Copy that file securely to the web server outside public-web, then set KFX_DB_CONFIG to its path.'
