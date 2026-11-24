from pathlib import Path
import subprocess,shlex
r=Path(__file__).resolve().parents[2];b=r/'build-mac-universal'
commands=subprocess.check_output(['ninja','-t','commands','keeper-enhanced'],cwd=b,text=True).splitlines()
compile=next(shlex.split(x) for x in commands if '/macos_game_options.cpp.o ' in x)
flags=[];i=1
while i<len(compile):
 x=compile[i]
 if x in ('-o','-MF','-MT'):i+=2;continue
 if x in ('-c','-MD','-DNDEBUG') or x.endswith('/macos_game_options.cpp'):i+=1;continue
 flags.append(x);i+=1
link=shlex.split(commands[-1]);libs=[];i=1
while i<len(link):
 x=link[i]
 if x=='-o':i+=2;continue
 if x.endswith('.o') or x in ('&&',':','-DNDEBUG','/usr/bin/c++'):i+=1;continue
 libs.append(x);i+=1
subprocess.run(['clang++',*flags,str(r/'macos/tests/test-ingame-options.cpp'),str(b/'CMakeFiles/keeper-enhanced.dir/src/macos_controls.c.o'),str(b/'CMakeFiles/keeper-enhanced.dir/src/macos_save_compat.cpp.o'),str(b/'CMakeFiles/keeper-enhanced.dir/src/macos_replay.cpp.o'),str(b/'CMakeFiles/keeper-enhanced.dir/src/net_hub.cpp.o'),str(b/'CMakeFiles/keeper-enhanced.dir/src/frontmenu_options.c.o'),str(b/'CMakeFiles/keeper-enhanced.dir/src/front_input.c.o'),'-Wl,-dead_strip',*libs,'-o','/tmp/test-ingame-options'],cwd=b,check=True)
import os
subprocess.run(['/tmp/test-ingame-options'],env=dict(os.environ,KFX_TEST_ART=str(r/'macos/packaging/controllers')),check=True)
