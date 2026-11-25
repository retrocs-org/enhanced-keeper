#!/usr/bin/env python3
"""Optionally RNC-compress an asset without risking the generated raw input."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile

def main():
    if len(sys.argv)!=2:raise SystemExit('Usage: rnc-asset FILE')
    original=Path(sys.argv[1]);tool=Path(os.environ.get('KFX_RNC_BINARY',str(Path(__file__).with_name('rnc'))))
    with tempfile.TemporaryDirectory(prefix='.rnc-',dir=original.parent) as tmp:
        staged=Path(tmp)/original.name;shutil.copyfile(original,staged)
        result=subprocess.run([str(tool),str(staged)],capture_output=True,text=True)
        if result.returncode==0:staged.replace(original)
        else:print('Kept uncompressed engine-compatible asset: '+str(original))
if __name__=='__main__':main()
