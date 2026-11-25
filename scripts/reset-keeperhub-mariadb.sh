#!/bin/sh
# Run on the MariaDB server with local socket access as a database administrator.
set -eu
umask 077

database=keeperfx_e
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
schema="$script_dir/mariadb.sql"
if [ -f "$script_dir/../public-web/lib/mariadb.sql" ]; then
    schema="$script_dir/../public-web/lib/mariadb.sql"
fi
apply=false
empty=false

usage() {
    echo "Usage: $0 [--schema /path/to/mariadb.sql | --empty] [--reset keeperfx_e]"
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --schema)
            [ "$#" -ge 2 ] || { usage >&2; exit 2; }
            schema=$2
            shift 2
            ;;
        --empty)
            empty=true
            shift
            ;;
        --reset)
            [ "$#" -ge 2 ] && [ "$2" = "$database" ] || { usage >&2; exit 2; }
            apply=true
            shift 2
            ;;
        --help)
            usage
            exit 0
            ;;
        *)
            usage >&2
            exit 2
            ;;
    esac
done

if [ "$empty" = false ]; then
    [ -f "$schema" ] && [ -r "$schema" ] || {
        echo "KeeperHub schema is missing or unreadable: $schema" >&2
        exit 1
    }
    for table in users cloud_profiles cloud_email_changes servers matches; do
        grep -Eq "^CREATE TABLE IF NOT EXISTS $table\(" "$schema" || {
            echo "The schema is missing KeeperHub's $table table. Use lib/mariadb.sql from the deployed revision." >&2
            exit 1
        }
    done
fi

for program in mariadb mariadb-dump; do
    command -v "$program" >/dev/null || { echo "Missing command: $program" >&2; exit 1; }
done
server=$(mariadb --protocol=socket --batch --skip-column-names --database="$database" \
    --execute="SELECT @@hostname")
if [ "$empty" = true ]; then schema='(initialize later on the web host)'; fi
printf 'MariaDB server: %s\nDatabase: %s\nSchema: %s\n' "$server" "$database" "$schema"

if [ "$apply" = false ]; then
    echo 'Preview only. To back up, drop, and recreate the database, pass --reset keeperfx_e.'
    exit 0
fi

backup_dir=/var/backups/keeperhub
install -d -m 700 "$backup_dir"
backup=$(mktemp "$backup_dir/keeperfx_e-$(date +%Y%m%d-%H%M%S).XXXXXXXX.sql")
if ! mariadb-dump --protocol=socket --single-transaction --routines --events --triggers \
    --databases "$database" --add-drop-database > "$backup"; then
    rm -f -- "$backup"
    echo 'Backup failed. Reset stopped.' >&2
    exit 1
fi
if ! grep -q '^-- Dump completed' "$backup"; then
    echo "Backup completion marker missing. Reset stopped. Inspect $backup." >&2
    exit 1
fi
printf 'Backup saved: %s\n' "$backup"

if ! mariadb --protocol=socket <<'SQL'
DROP DATABASE `keeperfx_e`;
CREATE DATABASE `keeperfx_e` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
SQL
then
    echo "Could not recreate the database. Review the error and restore from $backup if needed." >&2
    exit 1
fi

if [ "$empty" = true ]; then
    mariadb --protocol=socket --database="$database" \
        --execute='SELECT COUNT(*) AS tables_remaining FROM information_schema.tables WHERE table_schema=DATABASE();'
    echo 'Empty database created. Run lib/bin/migrate.php on the web host before restoring traffic.'
else
    if ! mariadb --protocol=socket --database="$database" < "$schema"; then
        echo "Schema import failed. Restore from $backup or fix the schema and rerun its import." >&2
        exit 1
    fi

    mariadb --protocol=socket --database="$database" \
        --execute='SELECT COUNT(*) AS users FROM users; SELECT COUNT(*) AS servers FROM servers;'
fi
printf 'Reset complete. To restore the old database: mariadb --protocol=socket < %s\n' "$backup"
