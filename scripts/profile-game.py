#!/usr/bin/env python3
"""Measure an isolated real dungeon without changing the player's saves/settings."""
import argparse
import json
import os
from pathlib import Path
import plistlib
import re
import resource
import shutil
import signal
import socket
import subprocess
import tempfile
import time


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app',type=Path,required=True)
    parser.add_argument('--engine',type=Path,required=True)
    parser.add_argument('--game-dir',type=Path,required=True)
    parser.add_argument('--save',type=Path)
    parser.add_argument('--seconds',type=int,default=30)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if not 10<=args.seconds<=300 or args.output.exists():parser.error('Choose 10–300 seconds and a new output file')
    metadata=plistlib.loads((args.app/'Contents/Info.plist').read_bytes())
    launcher=args.app/'Contents/MacOS'/metadata['CFBundleExecutable']
    with tempfile.TemporaryDirectory(prefix='keeperfx-profile-') as temporary:
        profile=Path(temporary);runtime=profile/'GameData';frames=profile/'frames.json'
        env=dict(os.environ,KFX_USER_DATA_DIR=str(profile),KFX_AUTOSAVE_SECONDS='0',KFX_FRAME_PROFILE=str(frames),KFX_APP_RESOURCES=str(args.app/'Contents/Resources'),ALSOFT_DRIVERS='null')
        for flags in (['--import',str(args.game_dir.resolve())],['--prepare-only']):
            subprocess.run([str(launcher),*flags],env=env,check=True,stdout=subprocess.DEVNULL)
        with socket.socket() as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
        cfg=runtime/'keeperfx.cfg';text=cfg.read_text()
        for key,value in {'API_ENABLED':'TRUE','API_PORT':str(port),'FRONTEND_RES':'640x480w32 1280x800w32 1280x800w32','INGAME_RES':'1280x800w32','FREEZE_GAME_ON_FOCUS_LOST':'FALSE'}.items():
            text=re.sub(r'(?m)^'+key+r'=.*$',key+'='+value,text)
        cfg.write_text(text)
        if args.save:shutil.copy2(args.save,profile/'save/fx1g0000.sav')
        baseline=resource.getrusage(resource.RUSAGE_CHILDREN)
        process=subprocess.Popen([str(runtime/'keeperfx'),'-nointro','-nosound','-level','1'],executable=str(args.engine.resolve()),cwd=runtime,env=env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,start_new_session=True)
        connection=None;began=time.monotonic()
        try:
            for _ in range(200):
                if process.poll() is not None:raise RuntimeError('Engine exited before profiling')
                try:connection=socket.create_connection(('127.0.0.1',port),timeout=.2);break
                except OSError:time.sleep(.1)
            if not connection:raise RuntimeError('Engine API did not start')
            time.sleep(3)
            if args.save:
                connection.sendall((json.dumps({'action':'console_command','player':0,'command':'game.load 0','ack':1})+'\n').encode())
                connection.settimeout(5);reply=json.loads(connection.recv(8192))
                if not reply.get('success'):raise RuntimeError('Saved dungeon could not be loaded')
                time.sleep(2)
            deadline=time.monotonic()+args.seconds
            while time.monotonic()<deadline:
                if process.poll() is not None:raise RuntimeError('Engine exited during capture')
                time.sleep(.25)
        finally:
            if connection:connection.close()
            if process.poll() is None:
                os.killpg(process.pid,signal.SIGTERM)
                try:process.wait(timeout=5)
                except subprocess.TimeoutExpired:os.killpg(process.pid,signal.SIGKILL);process.wait()
        usage=resource.getrusage(resource.RUSAGE_CHILDREN)
        report={'scenario':'saved dungeon' if args.save else 'campaign level 1','measurement_seconds':args.seconds,'process_elapsed_seconds':round(time.monotonic()-began,3),'process_cpu_seconds':round(usage.ru_utime+usage.ru_stime-baseline.ru_utime-baseline.ru_stime,3),'peak_rss_bytes':usage.ru_maxrss}
        if frames.exists():report['rendering']=json.loads(frames.read_text())
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps(report,indent=2))

if __name__=='__main__':main()
