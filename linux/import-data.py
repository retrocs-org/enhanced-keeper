#!/usr/bin/env python3
"""Import an already prepared, matching game-data directory. Never downloads assets."""
import argparse, shutil
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('game_data',type=Path,help='Prepared game directory containing keeperfx.cfg, data and campaigns')
p.add_argument('profile',type=Path)
a=p.parse_args();source=a.game_data.resolve();profile=a.profile.resolve();dest=profile/'GameData'
if not (source/'keeperfx.cfg').is_file():p.error('Select a prepared game directory containing keeperfx.cfg')
if dest.exists():p.error('GameData already exists; move it aside before an intentional re-import')
if profile==source or source in profile.parents or profile in source.parents:p.error('Source and profile must be separate trees')
# Reject links so imports cannot silently include files outside the selected tree.
if any(v.is_symlink() for v in source.rglob('*')):p.error('Source must not contain symlinks')
profile.mkdir(parents=True,exist_ok=True,mode=0o700)
shutil.copytree(source,dest,ignore=shutil.ignore_patterns('*.exe','*.dll','keeperfx','save','scrshots','recordings','community-session.cfg','*.log'))
for name in ('save','scrshots','recordings'):(profile/name).mkdir(exist_ok=True,mode=0o700)
print('Imported prepared game data. No assets are included in the server package.')
