#!/usr/bin/env python3
"""Import prepared game data for a dedicated server; never downloads assets."""
import argparse
import json
import os
from pathlib import Path
import shutil
import tempfile

ASSET_DIRS = {'data', 'fxdata', 'ldata', 'creatrs', 'campgns', 'levels', 'multiplayer', 'mods'}
PRIVATE = {'save', 'scrshots', 'recordings', 'logs', 'community-session.cfg', '.git', '.ds_store'}
BINARIES = {'keeperfx', 'keeper-enhanced', 'keeper-enhanced-hvlog'}
MEDIA = {'.wav', '.ogg', '.oga', '.mp3', '.flac', '.opus', '.mid', '.midi', '.smk', '.avi', '.mp4', '.mkv', '.webm', '.mov'}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('game_data', type=Path, help='Prepared matching game directory, not an original CD')
    p.add_argument('profile', type=Path)
    p.add_argument('--full', action='store_true', help='Keep standalone media and nonstandard asset directories too')
    p.add_argument('--dry-run', action='store_true', help='Report selection without creating files')
    a = p.parse_args()
    if a.game_data.is_symlink() or a.profile.is_symlink():
        p.error('Source and profile must not be symlinks')
    source, profile = a.game_data.resolve(), a.profile.resolve()
    dest = profile / 'GameData'
    if not any((source/n).is_file() for n in ('keeper-enhanced.cfg','keeperfx.cfg')):
        p.error('Select a prepared game directory containing keeper-enhanced.cfg')
    if dest.exists() or dest.is_symlink():
        p.error('GameData already exists; move it aside before an intentional re-import')
    if profile == source or source in profile.parents or profile in source.parents:
        p.error('Source and profile must be separate trees')
    selected = []
    omitted_bytes = 0
    for path in sorted(source.rglob('*')):
        if path.is_symlink():
            p.error('Source must not contain symlinks')
        if path.is_dir():
            continue
        if not path.is_file():
            p.error('Source contains a non-regular file')
        rel = path.relative_to(source)
        parts = [s.lower() for s in rel.parts]
        name = parts[-1]
        if rel.as_posix()=='keeperfx.cfg':
            if (source/'keeper-enhanced.cfg').exists(): continue
            rel=Path('keeper-enhanced.cfg')
        # Mods and all mapNNNNN.* companions are hashed byte-for-byte by readiness.
        protected = parts[0] == 'mods' or name.startswith('map') and name[3:8].isdigit() and name[8:9] == '.'
        private = any(s in PRIVATE for s in parts) or name.endswith(('.log', '.pck', '.sav', '.exe', '.dll', '.dylib', '.so')) or name in BINARIES
        allowed_root = parts[0] in ASSET_DIRS or rel.as_posix() == 'keeper-enhanced.cfg'
        media = path.suffix.lower() in MEDIA and not protected
        keep = not private and (a.full or allowed_root and not media)
        if keep:
            selected.append((path, rel))
        else:
            omitted_bytes += path.stat().st_size
    total = sum(path.stat().st_size for path, _ in selected)
    print(f'{"Full" if a.full else "Minimal"} server import: {len(selected)} files, {total} bytes; excluded {omitted_bytes} bytes.')
    if a.dry_run:
        return
    profile.mkdir(parents=True, exist_ok=True, mode=0o700)
    stage = Path(tempfile.mkdtemp(prefix='.server-import-', dir=profile))
    try:
        for path, rel in selected:
            target = stage / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(path, target)
        # Keep empty runtime asset directories for legacy directory scans.
        for name in ASSET_DIRS:
            (stage / name).mkdir(exist_ok=True)
        manifest = {'mode': 'full' if a.full else 'minimal', 'files': len(selected), 'bytes': total, 'excluded_bytes': omitted_bytes}
        (stage / 'server-import.json').write_text(json.dumps(manifest, indent=2) + '\n')
        if dest.exists() or dest.is_symlink():
            p.error('GameData appeared during import; refusing replacement')
        os.rename(stage, dest)
    finally:
        if stage.exists():
            shutil.rmtree(stage)
    for name in ('save', 'scrshots', 'recordings'):
        (profile / name).mkdir(exist_ok=True, mode=0o700)
    print('Imported. Retained engine bootstrap assets and complete mod content for compatibility.')


if __name__ == '__main__':
    main()
