#!/usr/bin/env python3
"""Guard against reintroducing a core-spinning macOS frame wait."""
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
deps=Path(os.environ.get('KFX_MAC_DEPS_ROOT',root/'build-macos-deps'))
build=Path(os.environ.get('KFX_MAC_BUILD_DIR',root/'build-mac-universal'))
flags=shlex.split(subprocess.check_output(['pkg-config','--static','--libs','sdl3'],text=True,env=dict(os.environ,PKG_CONFIG_PATH=str(deps/'sdl-prefix/lib/pkgconfig'))))
with tempfile.TemporaryDirectory() as temporary:
    target=Path(temporary)/'wait-test'
    subprocess.run(['clang++','-std=c++17','-Wl,-dead_strip','-I'+str(root/'src'),str(root/'macos/tests/test-frame-waits.cpp'),str(build/'CMakeFiles/keeperfx.dir/src/bflib_datetm.cpp.o'),*flags,'-o',str(target)],check=True)
    result=json.loads(subprocess.check_output([str(target)],text=True))
    assert result['cpu_seconds']<.5,result
    print('PASS: deadline waits sleep without busy-spinning:',result)
