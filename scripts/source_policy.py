"""Source repository policy: imported runtime content stays local."""
from pathlib import PurePosixPath
from release_support import ReleaseError

CONTENT_ROOTS = {'import', 'data', 'sound', 'music', 'speech', 'ldata', 'fxdata', 'gfx', 'sfx',
                 'campgns', 'levels', 'multiplayer', 'lang', 'config', 'res',
                 'save', 'screenshots', 'recordings', 'deps', 'external', 'third_party'}
CONTENT_SUFFIXES = {'.dat', '.fon', '.sbk', '.smk', '.raw', '.pal', '.wav', '.ogg',
                    '.mp3', '.flac', '.mid', '.midi', '.iso', '.cue', '.bin',
                    '.png', '.jpg', '.jpeg', '.bmp', '.gif', '.webp', '.ico', '.icns',
                    '.exe', '.dll', '.dylib', '.a', '.so', '.zip', '.7z', '.rar'}

def excluded(name):
    path = PurePosixPath(name)
    # Explicitly authorized GPL source asset trees; original disc imports stay excluded.
    if str(path).startswith(('assets/upstream/fxgraphics/', 'assets/upstream/fxsounds/')) and path.suffix.lower() not in {'.iso','.cue','.bin','.exe','.dll'}:
        return False
    # Prebuilt open assets are checked against their runtime manifest.
    if str(path).startswith('assets/runtime/') and path.suffix.lower() in {
        '.cfg', '.dat', '.fxfont', '.json', '.jty', '.lua', '.mp3', '.pal',
        '.png', '.raw', '.tab', '.toml', '.txt',
    }:
        return False
    if str(path) == 'public-web/assets/avatars/presets.png':
        return False
    # These are the controller-guide assets supplied for this project's UI.
    if str(path.parent) == 'macos/packaging/controllers' and path.suffix in {'.svg', '.png'}:
        return False
    return (bool(path.parts) and path.parts[0] in CONTENT_ROOTS
            or path.suffix.lower() in CONTENT_SUFFIXES
            or str(path).startswith(('docs/assets/', 'tools/local/')))

def validate_history(root, git):
    import importlib.util
    spec=importlib.util.spec_from_file_location('asset_provenance', __import__('pathlib').Path(__file__).with_name('check-asset-sources.py'))
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);module.verify(root)
    # Branch pushes include ancestors: deleting or ignoring an asset is insufficient.
    paths = git(root, 'log', '--format=', '--name-only', '-z', '--full-history', '--no-renames', '--root', 'HEAD')
    for name in paths.split('\0'):
        name = name.lstrip('\n')
        if name and excluded(name):
            raise ReleaseError('Imported/vendor content in source history: '+name+
                               '; use scripts/export_source.py to create a source-only checkout')
