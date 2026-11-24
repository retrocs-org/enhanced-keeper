#!/usr/bin/env python3
"""Manual native acceptance test for network reservation and authenticated resume."""
from pathlib import Path
import shlex
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2];build=root/'build-mac-universal'
commands=subprocess.check_output(['ninja','-t','commands','keeperfx'],cwd=build,text=True).splitlines()
command=next(shlex.split(row) for row in commands if '/bflib_enet.cpp.o ' in row)
flags=[];index=1
while index<len(command):
 value=command[index]
 if value in ('-o','-MF','-MT'):index+=2;continue
 if value in ('-c','-MD','-DNDEBUG') or value.endswith('/bflib_enet.cpp'):index+=1;continue
 flags.append(value);index+=1
with tempfile.TemporaryDirectory() as temporary:
 target=Path(temporary)/'network-resume'
 subprocess.run(['clang++',*flags,str(root/'macos/tests/test-network-resume.cpp'),'-Wl,-dead_strip',str(build/'enet6-build/libenet6.a'),'-o',str(target)],check=True)
 subprocess.run([str(target)],check=True)
