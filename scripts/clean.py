#!/usr/bin/env python3
"""Remove standard generated builds and imported runtime copies, preserving inputs."""
from pathlib import Path
import argparse
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[1]

def clean(root, dry_run=False):
    tracked=subprocess.check_output(['git','-C',str(root),'ls-files','-z'],text=True).split('\0')
    names={'tools/local','out','outputs','bin','obj','pkg','sdl','gfx','sfx',
           '.local/generated-runtime','.local/imported-client','.local/linux-server-import','.local/keeperfx','.local/runtime-cache'}
    names.update(p.name for p in root.glob('build-*') if p.is_dir() or p.is_symlink())
    candidates=[]
    for name in sorted(names):
        path=root/name
        if not path.exists() and not path.is_symlink():continue
        if any(t==name or t.startswith(name+'/') for t in tracked if t):
            raise RuntimeError('Refusing to delete tracked source: '+name)
        # Preserve locally used server/client state rather than silently erase it.
        if name.startswith('.local/') and not path.is_symlink():
            for state in ('save','scrshots','recordings','dedicated'):
                folder=path/state
                if folder.is_symlink() or folder.is_file() or folder.is_dir() and any(folder.iterdir()):
                    raise RuntimeError('Imported runtime contains personal state; move it aside before cleaning: '+str(folder))
        candidates.append(path)
    for path in candidates:
        print(('Would remove ' if dry_run else 'Removing ')+str(path.relative_to(root)))
        if not dry_run:
            if path.is_symlink() or path.is_file():path.unlink()
            else:shutil.rmtree(path)
    print('Kept original import/ discs, source, Git history and external installations.')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--dry-run',action='store_true')
    a=p.parse_args()
    try:clean(ROOT,a.dry_run)
    except (RuntimeError,subprocess.CalledProcessError) as error:p.exit(1,str(error)+'\n')

if __name__=='__main__':main()
