#!/usr/bin/env python3
"""Refresh a dedicated vhost from public-web, preserving its config.inc.php. No build step."""
import argparse
import os
from pathlib import Path
import pwd
import grp
import sys
import tempfile

SOURCE = Path(__file__).resolve().parents[1] / 'public-web'
MARKER = '.keeperfx-e-vhost'
SIGNATURE = 'KeeperFX-E public-web v1\n'
CONFIG = 'config.inc.php'


def fail(message):
    raise ValueError(message)


def refresh(target, apply=False, initialize=False, owner=None, group=None, set_permissions=False):
    if (owner is None) != (group is None):
        fail('Specify both --owner and --group.')
    identity = None
    if owner is not None:
        try:
            identity = (pwd.getpwnam(owner).pw_uid, grp.getgrnam(group).gr_gid)
        except KeyError:
            fail('The requested owner/group does not exist on this server.')
        if apply and os.geteuid() != 0 and (identity[0] != os.geteuid() or identity[1] not in [os.getegid(), *os.getgroups()]):
            fail('Run with sudo to set the requested vhost ownership.')
    if target.is_symlink():
        fail('The vhost must be a real directory, not a symlink.')
    target = target.resolve()
    source = SOURCE.resolve()
    if (target == Path(target.anchor) or target == Path.home().resolve()
            or target == source or target in source.parents or source in target.parents):
        fail('Choose a separate, dedicated vhost directory.')
    # A symlink anywhere below the vhost could otherwise redirect a write or deletion.
    if target.exists() and not target.is_dir():
        fail('The destination is not a directory.')
    entries = list(target.rglob('*')) if target.exists() else []
    if any(p.is_symlink() for p in entries):
        fail('The vhost contains symlinks; no files were changed.')
    marker = target / MARKER
    if marker.exists():
        if not marker.is_file() or marker.read_text() != SIGNATURE:
            fail('The vhost ownership marker is invalid.')
    elif not initialize or entries:
        fail('First deployment requires --initialize and a new or empty vhost directory.')
    files = {p.relative_to(source).as_posix(): p for p in source.rglob('*') if p.is_file()}
    if any(p.is_symlink() for p in source.rglob('*')):
        fail('The source payload must not contain symlinks.')
    preserve = (target / CONFIG).is_file()
    if (target / CONFIG).exists() and not preserve:
        fail('The deployed configuration is not a regular file.')
    keep = set(files) | {MARKER}
    stale = [p for p in entries if p.is_file() and p.relative_to(target).as_posix() not in keep]
    updates = []
    for name, src in files.items():
        if name == CONFIG and preserve:
            continue
        dest = target / name
        if dest.exists() and not dest.is_file():
            fail('A destination path conflicts with a source file: ' + name)
        if not dest.exists() or src.read_bytes() != dest.read_bytes():
            updates.append((name, src))
    print(('Refresh' if apply else 'Preview') + ': ' + str(target))
    print(f'{len(updates)} files to copy; {len(stale)} obsolete files to remove.')
    print('config.inc.php: ' + ('contents preserved' if preserve else 'installing credential-free defaults'))
    if identity:
        print('Ownership: ' + owner + ':' + group + ' (dedicated document root and its contents only)')
    if set_permissions:
        print('Permissions: directories 0755, files 0644, config.inc.php 0640')
    if not apply:
        print('No changes made. Add --apply to refresh this dedicated vhost.')
        return
    target.mkdir(parents=True, exist_ok=True)
    # Copy atomically per file; never print config content or copy local runtime state.
    for name, src in updates:
        dest = target / name
        dest.parent.mkdir(parents=True, exist_ok=True)
        fd, temporary = tempfile.mkstemp(prefix='.kfx-web-', dir=dest.parent)
        try:
            with os.fdopen(fd, 'wb') as out:
                out.write(src.read_bytes())
            os.chmod(temporary, 0o640 if name == CONFIG else 0o644)
            os.replace(temporary, dest)
        finally:
            if os.path.exists(temporary):
                os.unlink(temporary)
    for path in stale:
        path.unlink()
    for directory in sorted((p for p in target.rglob('*') if p.is_dir()), key=lambda p: len(p.parts), reverse=True):
        if not any(directory.iterdir()):
            directory.rmdir()
    marker.write_text(SIGNATURE)
    marker.chmod(0o644)
    # Permission changes are opt-in; the ordinary refresh preserves existing config mode.
    managed = [target, *target.rglob('*')]
    for path in managed:
        if path.is_symlink():
            fail('A symlink appeared during refresh; refusing permission changes.')
        if identity:
            os.chown(path, *identity)
        if set_permissions:
            path.chmod(0o755 if path.is_dir() else 0o640 if path == target / CONFIG else 0o644)
    print('Vhost refreshed. No migrations, builds, accounts or service restarts were performed.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('destination', type=Path, help='Dedicated local vhost document root (run on the web server).')
    parser.add_argument('--apply', action='store_true', help='Apply the refresh; default is preview only.')
    parser.add_argument('--initialize', action='store_true', help='Allow the first deployment into a new/empty directory.')
    parser.add_argument('--owner', help='Existing server account for all deployed files; requires --group.')
    parser.add_argument('--group', help='Existing server group for all deployed files; requires --owner.')
    parser.add_argument('--set-permissions', action='store_true', help='Set directories 0755, files 0644 and config 0640.')
    args = parser.parse_args()
    try:
        refresh(args.destination, args.apply, args.initialize, args.owner, args.group, args.set_permissions)
    except (ValueError, OSError) as error:
        print('Refresh refused: ' + str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
