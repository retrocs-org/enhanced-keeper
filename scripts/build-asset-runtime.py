#!/usr/bin/env python3
"""Build native converters and generate runtime assets from pinned source inputs."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import sys

ROOT=Path(__file__).resolve().parents[1]
PREFIX=ROOT/'tools/local'
SOURCES={
 'keeperfx':('dkfans/keeperfx','392c7ebdbbd6da6e9596ac769edb4353a01dca5f'),
 'pngpal2raw':('dkfans/pngpal2raw','c6b1a52c5e0cf1bdc588f1a4b4d6d37e8fed61bd'),
 'png2bestpal':('dkfans/png2bestpal','a8c54b8a2516e9d58fef696ef3c888fbc44c083c'),
 'po2ngdat':('dkfans/po2ngdat','00f90177ee4266da6766d427066a080a3ee2f686'),
 'sndbanker':('dkfans/sndbanker','b4cb6bd5d6c2d5ebaef23074453f94aacbcceabd'),
 'rnctools':('dkfans/rnctools','338b795df97b359ea8b25db62b0d4ebfd5d983a3'),
 'zlib':('madler/zlib','51b7f2abdade71cd9bb0e7a373ef2610ec6f9daf'),
 'libpng':('pnggroup/libpng','cd952f49f95bb27154ae77dbb103032d95f6e580'),
}

def run(args,**kwargs):
    print('+ '+' '.join(map(str,args)),flush=True)
    subprocess.run(list(map(str,args)),check=True,**kwargs)

def checkout(name,offline):
    repo,revision=SOURCES[name];path=PREFIX/'src'/name
    if path.is_symlink():raise ValueError('Source checkout must not be linked')
    if (path/'.git').exists():
        current=subprocess.check_output(['git','-C',str(path),'rev-parse','HEAD'],text=True).strip()
        if current==revision:return path
        raise ValueError('Unexpected source revision; remove tools/local before rebuilding: '+name)
    if offline:raise ValueError('Offline source missing: '+name)
    path.mkdir(parents=True,exist_ok=True)
    run(['git','init','-q',path]);run(['git','-C',path,'fetch','-q','--depth','1','https://github.com/'+repo+'.git',revision]);run(['git','-C',path,'checkout','-q','--detach','FETCH_HEAD'])
    return path

def tools(jobs,offline):
    for program in ('git','cmake','make','cc','c++'):
        if not shutil.which(program):raise ValueError('Missing build prerequisite: '+program)
    native=PREFIX/'native';build=PREFIX/'build';(PREFIX/'bin').mkdir(parents=True,exist_ok=True)
    zlib=checkout('zlib',offline);png=checkout('libpng',offline)
    if not (native/'lib/libz.a').exists():
        run(['cmake','-S',zlib,'-B',build/'zlib','-G','Unix Makefiles','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_INSTALL_PREFIX='+str(native),'-DCMAKE_INSTALL_LIBDIR=lib'])
        run(['cmake','--build',build/'zlib','--parallel',jobs]);run(['cmake','--install',build/'zlib'])
    if not (native/'lib/libpng16.a').exists():
        run(['cmake','-S',png,'-B',build/'libpng','-G','Unix Makefiles','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_INSTALL_PREFIX='+str(native),'-DCMAKE_INSTALL_LIBDIR=lib','-DPNG_SHARED=OFF','-DPNG_STATIC=ON','-DPNG_TESTS=OFF','-DPNG_TOOLS=OFF','-DZLIB_LIBRARY='+str(native/'lib/libz.a'),'-DZLIB_INCLUDE_DIR='+str(native/'include')])
        run(['cmake','--build',build/'libpng','--parallel',jobs]);run(['cmake','--install',build/'libpng'])
    for name in ('pngpal2raw','png2bestpal','po2ngdat','sndbanker','rnctools'):
        source=checkout(name,offline)
        (source/'obj').mkdir(exist_ok=True);(source/'bin').mkdir(exist_ok=True)
        link=(str(native/'lib/libpng16.a')+' '+str(native/'lib/libz.a')+' -lm') if name.startswith('png') else '-lm'
        compiler='cc -fgnu89-inline' if name=='rnctools' else 'cc'
        stamp=source/'.native-build-flags'
        signature=compiler+'|'+link
        if not stamp.exists() or stamp.read_text()!=signature:
            for obj in (source/'obj').glob('*.o'):obj.unlink()
        run(['make','-C',source,'-j'+str(jobs),'all','CC='+compiler,'CPP=c++','LINKLIB='+link,'CXXINCS=-I'+str(native/'include'),'INCS=-I'+str(native/'include')])
        stamp.write_text(signature)
        for binary in (('rnc','dernc') if name=='rnctools' else (name,)):
            shutil.copy2(source/'bin'/binary,PREFIX/'bin'/binary)
    shutil.copy2(ROOT/'scripts/compress-asset.py',PREFIX/'bin/rnc-asset')
    (PREFIX/'bin/rnc-asset').chmod(0o755)
    shutil.copystat(PREFIX/'bin/rnc',PREFIX/'bin/rnc-asset')
    print('Native asset tools built under tools/local/bin.',flush=True)

def generate(output,jobs,offline):
    if output.exists() or output.is_symlink():raise ValueError('Runtime output exists; refusing replacement: '+str(output))
    tools(jobs,offline)
    source=checkout('keeperfx',offline)
    graphics=ROOT/'assets/upstream/fxgraphics';sounds=ROOT/'assets/upstream/fxsounds'
    if not (graphics/'LICENSE').is_file() or not (sounds/'LICENSE').is_file():raise ValueError('Vendored asset sources are missing')
    work=PREFIX/'build/runtime-data';work.mkdir(parents=True,exist_ok=True)
    for name in ('campgns','config','docs','lang','levels','multiplayer','version.mk'):
        target=work/name
        if not target.exists():target.symlink_to(source/name,target_is_directory=(source/name).is_dir())
    for name,target in [('gfx',graphics),('sfx',sounds)]:
        if not (work/name).exists():(work/name).symlink_to(target,target_is_directory=True)
    (work/'tools').mkdir(exist_ok=True)
    for name in ('png2bestpal','po2ngdat'):
        target=work/'tools'/name
        if not target.exists():target.symlink_to(PREFIX/'src'/name,target_is_directory=True)
    # Reuse the pinned upstream data recipes without its Windows engine build.
    recipe='''\n.DEFAULT_GOAL := runtime-data
RM = rm -f
MV = mv -f
CP = cp -f
MKDIR = mkdir -p
ECHO = @echo
BIN = ignored.exe
HVLOGBIN = ignored-hvlog.exe
VER_STRING = 1.4.0.0
CAMPAIGNS = $(patsubst campgns/%.cfg,%,$(wildcard campgns/*.cfg))
MAPPACKS = $(patsubst levels/%.cfg,%,$(filter-out %/personal.cfg,$(wildcard levels/*.cfg)))
LANGS = eng chi cht cze dut fre ger ita jpn kor lat pol rus spa swe
'''
    for variable,binary in [('PNGTORAW','pngpal2raw'),('PNGTOBSPAL','png2bestpal'),('POTONGDAT','po2ngdat'),('WAVTODAT','sndbanker'),('RNC','rnc-asset'),('DERNC','dernc')]:recipe+=variable+' = '+str(PREFIX/'bin'/binary)+'\n'
    for name in ('pkg_lang.mk','pkg_gfx.mk','pkg_sfx.mk','package.mk'):recipe+='include '+str(source/name)+'\n'
    recipe+='\nruntime-data: $(NGTEXTDATS) $(NCTEXTDATS) $(MPTEXTDATS) pkg-gfx $(NGSOUNDDATS) $(addprefix pkg/campgns/,$(LANDVIEWSPEECH))\n'
    # GNU xargs -d is not present on macOS; copy lists with a small Python helper.
    helper=work/'copy-speech.py';helper.write_text('import pathlib,shutil,sys\np=pathlib.Path(sys.argv[1]);out=pathlib.Path(sys.argv[2]);out.mkdir(parents=True,exist_ok=True)\nfor line in p.read_text().splitlines()[1:]:\n name=line.split("\\t")[0]\n if name:shutil.copy2(p.parent/name,out/pathlib.Path(name).name)\n')
    sfx=(source/'pkg_sfx.mk').read_text()
    sfx=sfx.replace('tail -n +2 "$$<" | cut -f1 | xargs -d \'\\n\' -I {} $(CP) "$$(<D)/{}" "$$@/"','python3 '+str(helper)+' "$$<" "$$@"')
    (work/'pkg_sfx_native.mk').write_text(sfx)
    recipe=recipe.replace(str(source/'pkg_sfx.mk'),str(work/'pkg_sfx_native.mk'))
    (work/'AssetBuild.mk').write_text(recipe)
    generated=work/'pkg';generated.mkdir(exist_ok=True)
    shutil.copytree(source/'config',generated,dirs_exist_ok=True)
    for folder in ('campgns','levels','multiplayer'):
        shutil.copytree(source/folder,generated/folder,dirs_exist_ok=True)
    run(['make','-C',work,'-f','AssetBuild.mk','-j'+str(jobs),'runtime-data'])
    generated=work/'pkg'
    fonts=PREFIX/'build/fonts';fonts.mkdir(parents=True,exist_ok=True)
    fontsrc=ROOT/'tools/fxfontmaker'
    run([sys.executable,fontsrc/'rescale_unifont_hex.py',fontsrc/'unifont-17.0.04.hex',fonts/'unifont12.hex'])
    run([sys.executable,fontsrc/'bdf_to_hex.py',fontsrc/'wenquanyi_9pt.bdf',fonts/'wenquanyi.hex'])
    run([sys.executable,fontsrc/'merge_hex.py',fonts/'unifont12.hex',fonts/'wenquanyi.hex',fonts/'merged12.hex'])
    for source_name,target,height in [('unifont-17.0.04.hex','font16.fxfont',16),('unifont_jp-17.0.04.hex','font16_jpn.fxfont',16),('unifont_t-17.0.04.hex','font16_cht.fxfont',16)]:
        run([sys.executable,fontsrc/'unifont_hex_to_binary.py',fontsrc/source_name,generated/'fxdata'/target,height])
    run([sys.executable,fontsrc/'unifont_hex_to_binary.py',fonts/'merged12.hex',generated/'fxdata/font12.fxfont',12])
    output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.generated-runtime-',dir=output.parent) as temp:
        stage=Path(temp)/'base';shutil.copytree(generated,stage)
        old=stage/'keeperfx.cfg';new=stage/'keeper-enhanced.cfg'
        if old.exists():old.rename(new)
        text=new.read_text();import re
        text=re.sub(r'(?mi)^FRONTEND_RES\s*=.*$','FRONTEND_RES=640x480w32 DESKTOP DESKTOP',text)
        text=re.sub(r'(?mi)^INGAME_RES\s*=.*$','INGAME_RES=DESKTOP DESKTOP_FULL 1920x1080x32',text)
        new.write_text(text)
        licenses=stage/'licenses';licenses.mkdir(exist_ok=True)
        shutil.copy2(graphics/'LICENSE',licenses/'FXGraphics-GPL-3.0.txt');shutil.copy2(sounds/'LICENSE',licenses/'FXsounds-GPL-3.0.txt')
        for art in (ROOT/'macos/packaging/controllers').glob('*.png'):shutil.copy2(art,stage/'fxdata'/art.name)
        asset_sources=json.loads((ROOT/'assets/upstream/asset-sources.json').read_text())
        (stage/'asset-build.json').write_text(json.dumps({'source':'generated locally','revisions':SOURCES,'assets':{k:v['revision'] for k,v in asset_sources.items()}},indent=2)+'\n')
        stage.rename(output)
    print('Generated runtime ready: '+str(output),flush=True)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,default=ROOT/'.local/generated-runtime')
    p.add_argument('--tools-only',action='store_true')
    p.add_argument('--offline',action='store_true')
    p.add_argument('--jobs',type=int,default=int(os.environ.get('KFX_JOBS','4')))
    a=p.parse_args()
    if a.jobs<1:p.error('jobs must be positive')
    try:
        if a.tools_only:tools(a.jobs,a.offline)
        else:generate(a.output,a.jobs,a.offline)
    except (ValueError,OSError,subprocess.CalledProcessError) as error:p.exit(1,str(error)+'\n')
if __name__=='__main__':main()
