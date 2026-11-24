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
    # These are the controller-guide assets supplied for this project's UI.
    if str(path.parent) == 'macos/packaging/controllers' and path.suffix in {'.svg', '.png'}:
        return False
    return (bool(path.parts) and path.parts[0] in CONTENT_ROOTS
            or path.suffix.lower() in CONTENT_SUFFIXES
            or str(path).startswith('docs/assets/'))

def validate_history(root, git):
    # Branch pushes include ancestors: deleting or ignoring an asset is insufficient.
    paths = git(root, 'log', '--format=', '--name-only', '-z', '--full-history', '--no-renames', '--root', 'HEAD')
    for name in paths.split('\0'):
        name = name.lstrip('\n')
        if name and excluded(name):
            raise ReleaseError('Imported/vendor content in source history: '+name+
                               '; use scripts/export_source.py to create a source-only checkout')
