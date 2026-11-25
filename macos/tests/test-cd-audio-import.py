#!/usr/bin/env python3
"""CD audio boundary tests using synthetic sectors, no proprietary fixtures."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
import wave

spec=importlib.util.spec_from_file_location('disc_import',Path(__file__).resolve().parents[1]/'import-local-data.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

class AudioTests(unittest.TestCase):
    def test_single_bin_indexes_and_pregaps(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t);(root/'disc.bin').write_bytes(b''.join(bytes([i])*2352 for i in range(8)))
            cue=root/'disc.cue';cue.write_text('FILE "disc.bin" BINARY\n TRACK 01 MODE1/2352\n INDEX 01 00:00:00\n TRACK 02 AUDIO\n PREGAP 00:02:00\n INDEX 01 00:00:02\n TRACK 03 AUDIO\n INDEX 00 00:00:04\n INDEX 01 00:00:05\n')
            dest=root/'out';dest.mkdir();self.assertEqual(m.import_audio(cue,dest),2)
            for number,data in [(2,b'\x02'*2352+b'\x03'*2352),(3,b'\x05'*2352+b'\x06'*2352+b'\x07'*2352)]:
                with wave.open(str(dest/f'music/track{number:02d}.wav')) as w:
                    self.assertEqual((w.getnchannels(),w.getsampwidth(),w.getframerate()),(2,2,44100))
                    self.assertEqual(w.readframes(w.getnframes()),data)

    def test_multiple_bins_stored_pregap(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t);(root/'data.bin').write_bytes(b'0'*2352);(root/'audio.bin').write_bytes(b'0'*2352+b'1'*2352)
            cue=root/'disc.cue';cue.write_text('FILE "data.bin" BINARY\n TRACK 01 MODE1/2352\n INDEX 01 00:00:00\nFILE "audio.bin" BINARY\n TRACK 02 AUDIO\n INDEX 00 00:00:00\n INDEX 01 00:00:01\n')
            self.assertEqual(m.cue_audio_tracks(cue),[(2,root/'audio.bin',2352,2352)])

    def test_reject_outside_file(self):
        with tempfile.TemporaryDirectory() as t:
            cue=Path(t)/'disc.cue';cue.write_text('FILE "../audio.bin" BINARY\n TRACK 02 AUDIO\n INDEX 01 00:00:00\n')
            with self.assertRaises(ValueError):m.cue_audio_tracks(cue)

    def test_reject_truncated_sector(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t);(root/'audio.bin').write_bytes(b'bad')
            cue=root/'disc.cue';cue.write_text('FILE "audio.bin" BINARY\n TRACK 02 AUDIO\n INDEX 01 00:00:00\n')
            with self.assertRaises(ValueError):m.cue_audio_tracks(cue)

class OriginalMapTests(unittest.TestCase):
    def test_disc_layouts_preserve_enhanced_scripts(self):
        disc=m.Disc.__new__(m.Disc)
        names=['keeper/'+n for n in m.REQUIRED]+['keeper/levels/map00001.slb','keeper/levels/map00001.txt','keeper/levels/map00052.slb']
        disc.files={name:(0,1) for name in names};disc.read=lambda offset,size:b'X'*size
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);script=root/'campgns/keeporig/map00001.txt';script.parent.mkdir(parents=True);script.write_text('Enhanced script')
            disc.extract(root)
            self.assertEqual(script.read_text(),'Enhanced script')
            self.assertEqual((root/'campgns/keeporig/map00001.slb').read_bytes(),b'X')
            self.assertEqual((root/'multiplayer/original/map00052.slb').read_bytes(),b'X')

if __name__=='__main__': unittest.main()
