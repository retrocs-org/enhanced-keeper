import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('banks', Path(__file__).resolve().parents[1]/'scripts/repair-sound-banks.py')
banks = importlib.util.module_from_spec(spec)
spec.loader.exec_module(banks)

class BankTests(unittest.TestCase):
    def test_catalog_points_to_riff(self):
        # One PCM payload and one fixed-width directory entry.
        sample = b'RIFF'+bytes(20)
        catalog = bytes(18)+struct.pack('<III2B',0,22050,len(sample),1,0)
        head = bytes(18)
        entries = bytearray(9*16)
        struct.pack_into('<4I', entries, 2*16, len(sample), 0, len(catalog), len(sample))
        offset = len(sample)+len(catalog)
        data = sample+catalog+head+entries+struct.pack('<I',offset)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'sound.dat'
            path.write_bytes(data)
            self.assertTrue(banks.valid_bank(path))
            path.write_bytes(b'FAIL'+data[4:])
            self.assertFalse(banks.valid_bank(path))
            path.write_bytes(data[:-4]+bytes(4))
            self.assertFalse(banks.valid_bank(path))

    def test_truncated_bank(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'sound.dat'
            path.write_bytes(b'bad')
            self.assertFalse(banks.valid_bank(path))

if __name__ == '__main__':
    unittest.main()
