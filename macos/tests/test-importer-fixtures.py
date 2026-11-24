from pathlib import Path
import os,tempfile,subprocess
root=Path(__file__).resolve().parents[2]
exe=Path(os.environ.get('KFX_TEST_BOOTSTRAP',Path('/Applications/Keeper-Enhanced.app/Contents/MacOS/KeeperFXE')))
with tempfile.TemporaryDirectory(prefix='kfx-import-fixture-') as folder:
 folder=Path(folder);prefs=folder/'Profile';resources=folder/'Resources';game=resources/'game';game.mkdir(parents=True)
 (game/'keeperfx.cfg').write_text('RENDERER=SOFTWARE\n');(game/'marker.dat').write_text('version one');(resources/'asset-version.txt').write_text('1')
 disc=folder/'Disc'/'KEEPER'
 for group,names in {'DATA':['bluepal.dat','bluepall.dat','dogpal.pal','hitpall.dat','lightng.pal','redpal.col','redpall.dat','slab0-0.dat','slab0-1.dat','vampal.pal','whitepal.col'],'SOUND':['atmos1.sbk','atmos2.sbk','bullfrog.sbk']}.items():
  (disc/group).mkdir(parents=True)
  for name in names:(disc/group/name.upper()).write_bytes(b'fixture')
 env=dict(os.environ,KFX_USER_DATA_DIR=str(prefs),KFX_RESOURCES_DIR=str(resources))
 def call(*args,ok=True):
  r=subprocess.run([str(exe),*map(str,args)],env=env,capture_output=True,text=True)
  assert (r.returncode==0)==ok,(r.returncode,r.stderr)
 call('--import',folder/'Disc');call('--prepare-only')
 runtime=prefs/'GameData';(runtime/'keeperfx.cfg').write_text('RENDERER=OPENGL\n');(runtime/'cache.dat').write_bytes(b'keep')
 (resources/'asset-version.txt').write_text('2');(game/'marker.dat').write_text('version two')
 call('--prepare-only')
 assert (runtime/'marker.dat').read_text()=='version two'
 assert (runtime/'keeperfx.cfg').read_text()=='RENDERER=OPENGL\n'
 assert (runtime/'cache.dat').read_bytes()==b'keep'
 (disc/'DATA/SLAB0-0.DAT').write_bytes(b'');call('--import',folder/'Disc',ok=False)
 assert (prefs/'OriginalData/data/slab0-0.dat').read_bytes()==b'fixture'
 print('PASS: uppercase mounted-CD layout, updates refresh assets and preserve settings/cache, zero-length file rejected without losing existing import')

 # Content profiles share only imported originals; runtime, saves and settings stay separate.
 (prefs/'save').mkdir(exist_ok=True);(prefs/'save/main-save.fixture').write_bytes(b'main save unchanged')
 alternative=prefs/'ContentProfiles/mod-test';(alternative/'EnhancedData/mods/example').mkdir(parents=True)
 (alternative/'EnhancedData/mods/example/config.cfg').write_text('first overlay')
 (prefs/'content-profile.cfg').write_text('mod-test\n');call('--prepare-only')
 assert (alternative/'GameData/data/slab0-0.dat').read_bytes()==b'fixture'
 assert (alternative/'GameData/mods/example/config.cfg').read_text()=='first overlay'
 assert (runtime/'keeperfx.cfg').read_text()=='RENDERER=OPENGL\n'
 (alternative/'EnhancedData/mods/example/config.cfg').unlink()
 (alternative/'EnhancedData/mods/example/new.cfg').write_text('replacement overlay')
 call('--prepare-only');assert not (alternative/'GameData/mods/example/config.cfg').exists()
 assert (alternative/'GameData/mods/example/new.cfg').read_text()=='replacement overlay'
 assert (prefs/'save/main-save.fixture').read_bytes()==b'main save unchanged'
 import shutil
 shutil.rmtree(alternative/'GameData/mods');linked_target=folder/'linked-target';linked_target.mkdir()
 (alternative/'GameData/mods').symlink_to(linked_target,target_is_directory=True)
 (alternative/'EnhancedData/mods/example/new.cfg').write_text('updated overlay content')
 call('--prepare-only',ok=False);assert not list(linked_target.iterdir())
 (alternative/'GameData/mods').unlink();call('--prepare-only')
 (prefs/'content-profile.cfg').write_text('../../escape\n');call('--prepare-only')
 outside=folder/'outside';outside.mkdir();(prefs/'ContentProfiles/linked').symlink_to(outside,target_is_directory=True)
 (prefs/'content-profile.cfg').write_text('linked\n');call('--prepare-only');assert not list(outside.iterdir())
 print('PASS: separate profile runtime/save paths, shared imported originals, overlay replacement/removal and invalid/link selection rejection')

 # Keep all user operations inside this fixture profile.
 (prefs/'content-profile.cfg').write_text('default\n')
 (prefs/'save/community-session.cfg').write_text('private token')
 call('--export-personal')
 import zipfile
 archives=list((prefs/'Exports').glob('*.zip'));assert len(archives)==1
 with zipfile.ZipFile(archives[0]) as archive:
  names=archive.namelist();assert any(x.endswith('main-save.fixture') for x in names)
  assert not any('community-session' in x or '/OriginalData/' in x for x in names)
 (prefs/'save/macos-options.cfg').write_text('display_mode=1\nresolution=7\nwindow_cursor_lock=1\n')
 (runtime/'keeperfx.cfg').write_text('FRONTEND_RES=DESKTOP\nINGAME_RES=DESKTOP DESKTOP\nRENDERER=OPENGL\n')
 (prefs/'display-startup.pending').write_text('startup')
 call('--prepare-only')
 assert 'FRONTEND_RES=800x600w32' in (runtime/'keeperfx.cfg').read_text()
 assert 'display_mode=0' in (prefs/'save/macos-options.cfg').read_text()
 assert list(runtime.glob('keeperfx.cfg.before-display-recovery-*'))
 (prefs/'reset-profile-next.cfg').write_text('reset')
 call('--prepare-only')
 assert not (prefs/'save/main-save.fixture').exists()
 assert list((prefs/'ProfileBackups').glob('*/save/main-save.fixture'))
 assert (prefs/'OriginalData/data/slab0-0.dat').read_bytes()==b'fixture'
 print('PASS: personal ZIP excludes tokens/originals; failed display startup restores windowed settings; profile reset preserves recoverable files and shared originals')
