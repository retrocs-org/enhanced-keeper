#!/usr/bin/env python3
"""Verify vendored GPL assets against their pinned upstream Git blob manifest."""
import hashlib
import json
from pathlib import Path
from release_support import ReleaseError

SOURCES={'fxgraphics':'bc1b78e81cc5cade97d9c262c175a4bea0ed3a0f','fxsounds':'5258a75829f205aa01d2e7889bf6481ef468a7a0'}
def verify(root):
    base=Path(root)/'assets/upstream';manifest=base/'asset-sources.json'
    if not base.exists():return
    if not manifest.is_file() or manifest.is_symlink():raise ReleaseError('Vendored assets require a regular provenance manifest')
    records=json.loads(manifest.read_text())
    if set(records)!=set(SOURCES):raise ReleaseError('Unexpected vendored asset repository')
    for name,revision in SOURCES.items():
        record=records[name];directory=base/name
        if record['revision']!=revision:raise ReleaseError('Unexpected asset revision: '+name)
        actual=set()
        for path in directory.rglob('*'):
            if path.is_symlink():raise ReleaseError('Linked asset source: '+str(path))
            if not path.is_file():continue
            relative=path.relative_to(directory).as_posix();actual.add(relative)
            if relative not in record['files']:raise ReleaseError('Unrecorded upstream asset: '+relative)
            data=path.read_bytes();sha=hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
            if sha!=record['files'][relative]:raise ReleaseError('Upstream asset changed without provenance update: '+relative)
        if actual!=set(record['files']):raise ReleaseError('Missing vendored asset files: '+name)
    runtime = Path(root) / 'assets/runtime'
    if runtime.exists() or runtime.is_symlink():
        manifest = runtime / 'manifest.json'
        if runtime.is_symlink() or manifest.is_symlink() or not manifest.is_file():
            raise ReleaseError('Prebuilt runtime requires a regular asset manifest')
        expected = json.loads(manifest.read_text())
        actual = {}
        for path in runtime.rglob('*'):
            if path.is_symlink():
                raise ReleaseError('Linked prebuilt asset: ' + str(path))
            if path.is_file() and path != manifest:
                actual[path.relative_to(runtime).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
        if actual != expected:
            raise ReleaseError('Prebuilt runtime differs from its asset manifest')

if __name__=='__main__':
    verify(Path(__file__).resolve().parents[1]);print('Vendored asset provenance verified.')
