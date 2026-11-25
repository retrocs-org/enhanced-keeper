#!/usr/bin/env python3
"""Repair locally generated LP64 sound banks without reimporting original discs."""
import argparse
from pathlib import Path
import os
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]

def valid_bank(path):
    try:
        data = path.read_bytes()
        head, = struct.unpack_from('<I', data, len(data)-4)
        catalog, base, size, _ = struct.unpack_from('<4I', data, head+18+2*16)
        if not size or size % 32 or catalog+size > head:
            return False
        for pos in range(catalog, catalog+size, 32):
            offset, = struct.unpack_from('<I', data, pos+18)
            if data[base+offset:base+offset+4] != b'RIFF':
                return False
        return True
    except (OSError, struct.error):
        return False

def repair(game):
    banks = list((game/'sound').glob('*.dat'))
    broken = [bank for bank in banks if not valid_bank(bank)]
    if not broken:
        return
    if not (game/'asset-build.json').is_file():
        raise ValueError('Invalid sound bank in a runtime not generated locally: '+str(broken[0]))
    print('Repairing generated sound banks from local GPL sources.', flush=True)
    with tempfile.TemporaryDirectory(prefix='keeper-sound-') as temporary:
        runtime = Path(temporary)/'runtime'
        command = [sys.executable, str(ROOT/'scripts/build-asset-runtime.py'), '--output', str(runtime), '--jobs', os.environ.get('KFX_JOBS','4')]
        if os.environ.get('KFX_ASSET_OFFLINE') == '1':
            command.append('--offline')
        subprocess.run(command, check=True)
        for bank in broken:
            source = runtime/'sound'/bank.name
            if not valid_bank(source):
                raise ValueError('Generated sound bank failed validation: '+bank.name)
        for bank in broken:
            stage = bank.with_suffix('.dat.repair')
            shutil.copy2(runtime/'sound'/bank.name, stage)
            stage.replace(bank)
    print('Repaired sound effects and speech banks.', flush=True)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', type=Path, required=True)
    args = parser.parse_args()
    repair(args.game)
