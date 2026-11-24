#!/usr/bin/env python3
"""Refresh generated minimal server data from a full import, never a live profile."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]
MARKER='.generated-server-data'

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();source=a.source.resolve();output=a.output.absolute()
    if not (source/'keeper-enhanced.cfg').is_file():p.error('Run make import first, or set SERVER_DATA_BASE to a full prepared import')
    if source==output or source in output.parents or output in source.parents:p.error('Import and generated data must be separate trees')
    if output.is_symlink():p.error('Generated destination must not be a symlink')
    if output.exists():
        if not (output/MARKER).is_file():p.error('Refusing to replace an unmarked destination')
        if any(v.is_symlink() for v in output.rglob('*')):p.error('Generated data contains symlinks')
        for name in ('save','scrshots','recordings','dedicated'):
            folder=output/name
            if folder.exists() and (not folder.is_dir() or any(folder.iterdir())):p.error('Destination contains runtime state; refusing replacement')
    output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.server-data-',dir=output.parent) as temp:
        temp=Path(temp);stage=temp/'new';previous=temp/'previous'
        subprocess.run([sys.executable,str(ROOT/'linux/import-data.py'),str(source),str(stage)],check=True)
        (stage/MARKER).write_text('Generated build data. Do not use as a live profile.\n')
        if output.exists():output.rename(previous)
        try:stage.rename(output)
        except Exception:
            if previous.exists():previous.rename(output)
            raise
    print(f'Server data staged in {output / "GameData"}. Full import unchanged.')

if __name__=='__main__':
    try:main()
    except subprocess.CalledProcessError as error:sys.exit(error.returncode)
