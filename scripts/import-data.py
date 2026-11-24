#!/usr/bin/env python3
"""Shared Make entry point for owner-supplied client and minimal server data."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('kind',choices=['client','server'])
    p.add_argument('--base',type=Path,required=True)
    p.add_argument('--import-dir',type=Path,required=True)
    p.add_argument('--image',default='')
    p.add_argument('--output',default='')
    a=p.parse_args()
    output=Path(a.output) if a.output else ROOT/'.local'/('imported-client' if a.kind=='client' else 'linux-server-import')
    if output.exists():p.error(f'{output} already exists; choose a new IMPORT_OUTPUT (nothing overwritten)')
    if not any((a.base/n).is_file() for n in ('keeper-enhanced.cfg','keeperfx.cfg')):
        p.error('IMPORT_BASE must point to a prepared matching runtime. Original discs do not contain Enhanced maps/configuration.')
    image=Path(a.image) if a.image else None
    directory=image.parent if image and image.is_absolute() else a.import_dir
    def client_args(destination):
        args=[sys.executable,str(ROOT/'macos/import-local-data.py'),'--base',str(a.base),'--import-dir',str(directory),'--output',str(destination)]
        if image: args+=['--image',str(image)]
        return args
    if a.kind=='client':
        subprocess.run(client_args(output),check=True)
        print(f'Client data ready: {output}')
    else:
        # An IMAGE overlays original files first; otherwise the prepared base is used directly.
        with tempfile.TemporaryDirectory(prefix='keeper-server-import-') as temp:
            base=a.base
            if image:
                base=Path(temp)/'prepared'
                subprocess.run(client_args(base)+['--no-audio'],check=True)
            subprocess.run([sys.executable,str(ROOT/'linux/import-data.py'),str(base),str(output)],check=True)
        print(f'Minimal server data ready: {output / "GameData"}')

if __name__=='__main__':
    try:main()
    except subprocess.CalledProcessError as error:sys.exit(error.returncode)
