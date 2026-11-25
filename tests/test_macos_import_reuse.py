"""Exercise the local macOS packaging wrapper without compiling the engine."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class ImportReuseTests(unittest.TestCase):
    def run_wrapper(self, image=None):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "macos").mkdir()
            (root / "scripts").mkdir()
            (root / "scripts/repair-sound-banks.py").write_text("pass")
            shutil.copyfile(ROOT / "macos/build-local-client.sh", root / "macos/build-local-client.sh")
            game = root / ".local/imported-client"
            game.mkdir(parents=True)
            (game / "keeper-enhanced.cfg").touch()
            base = root / ".local/generated-runtime"
            base.mkdir()
            (base / "keeper-enhanced.cfg").touch()
            (root / "macos/import-local-data.py").write_text('import sys; print("IMPORT", *sys.argv[1:])')
            for name, body in [("build-universal.sh", "echo BUILD"), ("package-app.sh", 'echo "PACKAGE=$KFX_GAME_DIR"')]:
                script = root / "macos" / name
                script.write_text("#!/bin/sh\n" + body + "\n")
                script.chmod(0o755)
            env = {key: value for key, value in os.environ.items() if not key.startswith("KFX_")}
            if image:
                env["KFX_IMPORT_IMAGE"] = image
            result = subprocess.run(["sh", str(root / "macos/build-local-client.sh")], env=env, capture_output=True, text=True, check=True)
            return result.stdout

    def test_existing_import_is_reused(self):
        output = self.run_wrapper()
        self.assertNotIn("IMPORT ", output)
        self.assertIn(".local/imported-client", output)
        self.assertIn("BUILD", output)

    def test_explicit_image_imports_again(self):
        output = self.run_wrapper("Dungeon Keeper.cue")
        self.assertIn("IMPORT --image Dungeon Keeper.cue", output)
        self.assertIn("/imported-game", output)
        self.assertNotIn("Reusing", output)

if __name__ == "__main__":
    unittest.main()
