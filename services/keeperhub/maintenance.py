#!/usr/bin/env python3
"""Consistent online SQLite backups and read-only health verification."""
from contextlib import closing
import argparse
import os
from pathlib import Path
import sqlite3
import tempfile
from datetime import datetime, timezone


def verify(database):
    if database.is_symlink() or not database.is_file():
        raise ValueError('Choose an existing regular database')
    with closing(sqlite3.connect(database.resolve().as_uri()+'?mode=ro', uri=True)) as db:
        if db.execute('PRAGMA quick_check').fetchall() != [('ok',)]:
            raise ValueError('Database integrity check failed')
        if not db.execute("SELECT 1 FROM sqlite_master WHERE type='table' AND name='users'").fetchone():
            raise ValueError('Not a KeeperHub database')


def backup(database, folder, keep=7):
    verify(database)
    if folder.is_symlink() or not 1 <= keep <= 365:
        raise ValueError('Invalid backup destination or retention')
    folder.mkdir(mode=0o700, parents=True, exist_ok=True)
    if database.resolve().parent == folder.resolve():
        raise ValueError('Keep backups in a separate private folder')
    descriptor, name = tempfile.mkstemp(prefix='.backup-', dir=folder)
    os.close(descriptor)
    temporary = Path(name)
    try:
        with closing(sqlite3.connect(database.resolve().as_uri()+'?mode=ro', uri=True)) as source, closing(sqlite3.connect(temporary)) as target:
            source.backup(target)
            target.execute('PRAGMA journal_mode=DELETE')
        verify(temporary)
        stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
        destination = folder/('keeperhub-'+stamp+'.sqlite3')
        temporary.replace(destination)
        # Only prune this tool's own named regular backups.
        import re
        old = sorted(p for p in folder.iterdir() if not p.is_symlink() and p.is_file()
                     and re.fullmatch(r'keeperhub-\d{8}T\d{12}Z\.sqlite3', p.name))
        for path in old[:-keep]:
            path.unlink()
        return destination
    finally:
        temporary.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=['check', 'backup'])
    parser.add_argument('--db', required=True, type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--keep', type=int, default=7)
    args = parser.parse_args()
    if args.command == 'check':
        verify(args.db)
        print('Database integrity verified.')
    else:
        if not args.output:
            parser.error('backup requires --output outside the website document root')
        print('Verified private backup: '+str(backup(args.db,args.output,args.keep)))

if __name__ == '__main__':
    main()
