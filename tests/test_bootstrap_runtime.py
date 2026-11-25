import importlib.util
from pathlib import Path
import unittest

spec=importlib.util.spec_from_file_location('bootstrap',Path(__file__).resolve().parents[1]/'scripts/bootstrap-runtime.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

class ArchiveTests(unittest.TestCase):
    def test_regular_asset_paths(self):
        m.validate_listing('archive header\n----------\nPath = data/slab.dat\nPath = ./keeperfx.cfg\n')

    def test_reject_traversal_and_absolute_paths(self):
        for name in ('../outside','/etc/passwd','C:/outside','data/../../outside','..\\outside'):
            with self.subTest(name=name),self.assertRaises(ValueError):
                m.validate_listing('----------\nPath = '+name+'\n')

    def test_reject_links(self):
        for field in ('Symbolic Link = outside','Hard Link = outside'):
            with self.subTest(field=field),self.assertRaises(ValueError):
                m.validate_listing('----------\nPath = data/file\n'+field+'\n')

    def test_require_parseable_listing(self):
        with self.assertRaises(ValueError):m.validate_listing('not a 7z listing')

if __name__=='__main__':unittest.main()
