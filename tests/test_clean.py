import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec=importlib.util.spec_from_file_location('cleanup',Path(__file__).resolve().parents[1]/'scripts/clean.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

class CleanTests(unittest.TestCase):
    def test_clean_and_preview_preserve_inputs(self):
        with tempfile.TemporaryDirectory() as tmp, patch.object(m.subprocess,'check_output',return_value='build/cmake/Keep.cmake\0'):
            root=Path(tmp)
            for name in ('out/test','outputs/app','build-macos-deps/lib','build/cmake/Keep.cmake','import/disc.bin','.local/imported-client/data/file','.local/keeperfx/data/file','.local/runtime-cache/archive.7z'):
                p=root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('test')
            m.clean(root,True);self.assertTrue((root/'out/test').exists())
            m.clean(root)
            for name in ('out','outputs','build-macos-deps','.local/imported-client','.local/keeperfx','.local/runtime-cache'):self.assertFalse((root/name).exists())
            for name in ('import/disc.bin','build/cmake/Keep.cmake'):self.assertTrue((root/name).exists())

    def test_refuses_personal_state_before_deleting(self):
        with tempfile.TemporaryDirectory() as tmp, patch.object(m.subprocess,'check_output',return_value=''):
            root=Path(tmp);(root/'out').mkdir();p=root/'.local/linux-server-import/save/progress.sav';p.parent.mkdir(parents=True);p.write_text('keep')
            with self.assertRaises(RuntimeError):m.clean(root)
            self.assertTrue((root/'out').exists());self.assertEqual(p.read_text(),'keep')

    def test_refuses_tracked_outputs(self):
        with tempfile.TemporaryDirectory() as tmp, patch.object(m.subprocess,'check_output',return_value='out/keep.txt\0'):
            root=Path(tmp);(root/'out').mkdir()
            with self.assertRaises(RuntimeError):m.clean(root)
            self.assertTrue((root/'out').exists())

    def test_unlinks_output_symlink_without_deleting_target(self):
        with tempfile.TemporaryDirectory() as tmp, patch.object(m.subprocess,'check_output',return_value=''):
            root=Path(tmp);external=root/'keep';external.mkdir();(external/'file').write_text('keep');(root/'out').symlink_to(external,target_is_directory=True)
            m.clean(root);self.assertFalse((root/'out').exists());self.assertTrue((external/'file').exists())

if __name__=='__main__':unittest.main()
