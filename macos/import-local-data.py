#!/usr/bin/env python3
"""Overlay required owner-supplied CD files onto a prepared Keeper Enhanced runtime."""
import argparse
from pathlib import Path
import re
import shutil
import struct
import tempfile
import wave
import json

REQUIRED = ['data/' + n for n in ('bluepal.dat','bluepall.dat','dogpal.pal','hitpall.dat','lightng.pal','redpal.col','redpall.dat','slab0-0.dat','slab0-1.dat','vampal.pal','whitepal.col')] + ['sound/' + n for n in ('atmos1.sbk','atmos2.sbk','bullfrog.sbk')]

class Disc:
    def __init__(self, image):
        if image.suffix.lower() == '.cue':
            text = image.read_text()
            match = re.search(r'FILE "([^"\n]+)" BINARY\s+TRACK 01 MODE1/2352\s+INDEX 01 00:00:00', text, re.I)
            if not match or Path(match[1]).name != match[1]:
                raise ValueError('Expected a local MODE1/2352 track 1 with INDEX 01 00:00:00')
            image = image.parent / match[1]
            if image.is_symlink(): raise ValueError('Disc track must not be a symlink')
            self.sector, self.offset = 2352, 16
        else:
            self.sector, self.offset = 2048, 0
        self.stream = image.open('rb')
        self.size = image.stat().st_size // self.sector * 2048
        pvd = self.read(16 * 2048, 2048)
        if pvd[:7] != b'\x01CD001\x01': raise ValueError('Unsupported disc: ISO9660 primary volume missing')
        self.files = {}
        self.walk(pvd[156:190], '', set())

    def read(self, offset, size):
        if offset < 0 or size < 0 or offset + size > self.size: raise ValueError('Invalid disc extent')
        result = bytearray()
        while size:
            block, within = divmod(offset, 2048)
            count = min(size, 2048-within)
            self.stream.seek(block*self.sector+self.offset+within)
            chunk = self.stream.read(count)
            if len(chunk) != count: raise ValueError('Truncated disc')
            result.extend(chunk); offset += count; size -= count
        return bytes(result)

    def walk(self, record, prefix, seen):
        extent, size = struct.unpack_from('<I', record, 2)[0], struct.unpack_from('<I', record, 10)[0]
        if extent in seen or len(seen) > 4096: raise ValueError('Invalid disc directory tree')
        seen.add(extent)
        if size > 16*1024*1024: raise ValueError('Disc directory too large')
        data = self.read(extent*2048, size); pos = 0
        while pos < len(data):
            length = data[pos]
            if not length: pos = (pos//2048+1)*2048; continue
            rec = data[pos:pos+length]; pos += length
            if len(rec) < 34 or len(rec) != length: raise ValueError('Invalid disc directory record')
            raw = rec[33:33+rec[32]]
            if raw in (b'\0',b'\1'): continue
            name = raw.decode('ascii').split(';')[0].lower()
            if name in ('.','..') or '/' in name or '\\' in name: raise ValueError('Unsafe disc filename')
            path = prefix+name
            if rec[25]&2: self.walk(rec,path+'/',seen)
            else: self.files[path] = (struct.unpack_from('<I',rec,2)[0]*2048,struct.unpack_from('<I',rec,10)[0])

    def extract(self, destination):
        prefix = next((p for p in ('keeper/','') if all(p+n in self.files for n in REQUIRED)), None)
        if prefix is None: raise ValueError('Disc is missing required Dungeon Keeper files')
        names = REQUIRED + [n[len(prefix):] for n in self.files if n.startswith(prefix+'ldata/') and n.endswith('.smk')]
        for name in names:
            offset, size = self.files[prefix+name]
            if size == 0: raise ValueError('Empty required disc file: '+name)
            target = destination/name; target.parent.mkdir(parents=True,exist_ok=True)
            with target.open('wb') as output:
                while size:
                    count = min(size,1024*1024); output.write(self.read(offset,count)); offset += count; size -= count
        return len(names)


def cue_audio_tracks(cue):
    """Return byte ranges for AUDIO tracks, excluding stored/virtual pregaps."""
    tracks = []
    current_file = None
    current = None
    def sector(value):
        parts = value.split(':')
        if len(parts) != 3 or not all(p.isdigit() for p in parts):
            raise ValueError('Invalid CUE timestamp')
        minute, second, frame = map(int, parts)
        if second >= 60 or frame >= 75: raise ValueError('Invalid CUE timestamp')
        return (minute*60+second)*75+frame
    for line in cue.read_text().splitlines():
        line = line.strip()
        if line.upper().startswith('FILE '):
            match = re.fullmatch(r'FILE "([^"\n]+)" BINARY', line, re.I)
            if not match or Path(match[1]).name != match[1] or '\\' in match[1]:
                raise ValueError('CUE tracks must reference local BIN filenames')
            current_file = cue.parent / match[1]
            if current_file.is_symlink() or not current_file.is_file():
                raise ValueError('CUE track is missing or linked')
            current = None
        elif line.upper().startswith('TRACK '):
            match = re.fullmatch(r'TRACK (\d+) (AUDIO|MODE1/2352)',line,re.I)
            if not match or current_file is None: raise ValueError('Unsupported CUE track layout')
            current = {'file':current_file, 'number':int(match[1]), 'mode':match[2].upper(), 'indexes':{}}
            tracks.append(current)
        elif line.upper().startswith('INDEX '):
            match = re.fullmatch(r'INDEX (\d+) (\d+:\d+:\d+)',line,re.I)
            if not match or current is None: raise ValueError('Invalid CUE index')
            index = int(match[1])
            if index in current['indexes']: raise ValueError('Duplicate CUE index')
            current['indexes'][index] = sector(match[2])
    audio = []
    for i, track in enumerate(tracks):
        if track['mode'] != 'AUDIO': continue
        if 1 not in track['indexes']: raise ValueError('Audio track has no INDEX 01')
        start = track['indexes'][1]*2352
        end = track['file'].stat().st_size
        if i+1 < len(tracks) and tracks[i+1]['file'] == track['file']:
            following = tracks[i+1]['indexes']
            boundary = following.get(0, following.get(1))
            if boundary is None: raise ValueError('Next track is missing its boundary')
            end = boundary*2352
        if not 0 <= start < end <= track['file'].stat().st_size or (end-start)%2352:
            raise ValueError('Invalid or truncated CD audio extent')
        audio.append((track['number'], track['file'], start, end-start))
    if audio and [n for n, *_ in audio] != list(range(2,2+len(audio))):
        raise ValueError('Expected contiguous CD audio tracks starting at track 02')
    return audio


def import_audio(image, destination):
    tracks = cue_audio_tracks(image) if image.suffix.lower() == '.cue' else []
    music = destination/'music'
    music.mkdir(exist_ok=True)
    # The engine maps sorted files of its preferred format to CD track numbers.
    for old in music.iterdir():
        if old.suffix.lower() in {'.flac','.wav','.ogg','.mp3'}: old.unlink()
    for number, source, offset, size in tracks:
        target = music/f'track{number:02d}.wav'
        with source.open('rb') as stream, wave.open(str(target),'wb') as output:
            output.setnchannels(2); output.setsampwidth(2); output.setframerate(44100)
            stream.seek(offset)
            while size:
                chunk = stream.read(min(size,2352*512))
                if not chunk: raise ValueError('Truncated CD audio track')
                output.writeframesraw(chunk); size -= len(chunk)
        print(f'Imported CD audio track {number:02d}: {target.name}')
    (destination/'original-audio-files.json').write_text(json.dumps([f"track{n:02d}.wav" for n, *_ in tracks])+"\n")
    return len(tracks)


def select_image(directory, choice=None):
    directory = directory.resolve()
    if not directory.is_dir(): raise ValueError('Import directory does not exist')
    images = sorted(p for p in directory.iterdir() if p.suffix.lower() in ('.cue','.iso'))
    if choice:
        image = Path(choice)
        if not image.is_absolute(): image = directory / image
        if image.is_symlink(): raise ValueError('Disc image must not be a symlink')
        image = image.resolve()
        if image.parent != directory or image not in images:
            raise ValueError('Choose a CUE or ISO directly inside import/')
        return image
    if len(images) != 1:
        raise ValueError('Choose --image (or KFX_IMPORT_IMAGE for make client-macos). Available: ' + ', '.join(p.name for p in images))
    if images[0].is_symlink(): raise ValueError('Disc image must not be a symlink')
    return images[0]


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--base',type=Path,required=True,help='Prepared engine runtime, including enhanced assets and keeper-enhanced.cfg')
    p.add_argument('--import-dir',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--image',help='CUE or ISO filename inside import/; required when several discs are present')
    a=p.parse_args()
    if not any((a.base/n).is_file() for n in ('keeper-enhanced.cfg','keeperfx.cfg')): p.error('Prepared runtime missing; set KFX_GAME_DIR to its location')
    if a.output.exists(): p.error('Output already exists; choose a fresh output directory')
    if a.base.resolve() == a.output.resolve() or a.base.resolve() in a.output.resolve().parents:
        p.error('Output must be outside the base runtime')
    try: image = select_image(a.import_dir, a.image)
    except ValueError as error: p.error(str(error))
    if any(v.is_symlink() for v in a.base.rglob('*')): p.error('Inputs must not contain symlinks')
    print('Importing original files from: ' + image.name)
    a.output.parent.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.mac-import-',dir=a.output.parent) as temp:
        stage=Path(temp)/'game'
        shutil.copytree(a.base,stage,ignore=shutil.ignore_patterns('save','scrshots','recordings','*.log','community-session.cfg','keeperfx','keeper-enhanced','_CodeSignature','.DS_Store'))
        legacy=stage/'keeperfx.cfg'; config=stage/'keeper-enhanced.cfg'
        if not config.exists(): shutil.copyfile(legacy,config)
        legacy.unlink(missing_ok=True)
        disc=Disc(image)
        try: count=disc.extract(stage)
        finally: disc.stream.close()
        audio_count=import_audio(image, stage)
        stage.rename(a.output)
    print(f'Imported {count} original game/movie files and {audio_count} lossless CD audio tracks into {a.output}.')
    if image.suffix.lower()=='.iso': print('ISO images contain no CD-audio tracks; use the CUE/BIN source to import music.')

if __name__ == '__main__':
    main()
