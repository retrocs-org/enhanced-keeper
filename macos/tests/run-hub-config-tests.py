from pathlib import Path
import subprocess,shlex
r=Path(__file__).resolve().parents[2];b=r/'build-mac-universal'
commands=subprocess.check_output(['ninja','-t','commands','keeperfx'],cwd=b,text=True).splitlines()
a=next(shlex.split(x) for x in commands if '/net_hub.cpp.o ' in x);flags=[];i=1
while i<len(a):
 x=a[i]
 if x in ('-o','-MF','-MT'):i+=2;continue
 if x in ('-c','-MD','-DNDEBUG') or x.endswith('/net_hub.cpp'):i+=1;continue
 flags.append(x);i+=1
link=shlex.split(commands[-1]);libs=[];i=1
while i<len(link):
 x=link[i]
 if x=='-o':i+=2;continue
 if x.endswith('.o') or x in ('&&',':','-DNDEBUG','/usr/bin/c++'):i+=1;continue
 libs.append(x);i+=1
subprocess.run(['clang++',*flags,str(r/'macos/tests/test-hub-config.cpp'),str(r/'src/net_hub.cpp'),'-Wl,-dead_strip',*libs,'-o','/tmp/test-hub-config'],cwd=b,check=True)
subprocess.run(['/tmp/test-hub-config'],check=True)
