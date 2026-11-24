"""Check release-selected inputs without printing matched values."""
import re
from release_support import source_files,ReleaseError
PATTERNS=[rb'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----',rb'gh[pousr]_[A-Za-z0-9]{30,}',rb'github_pat_[A-Za-z0-9_]{40,}',rb'glpat-[A-Za-z0-9_-]{20,}']
def scan(root, files=None):
    for name,metadata in (source_files(root) if files is None else files).items():
        if 'gitlink' in metadata:continue
        p=root/name
        if p.stat().st_size>8*1024*1024:continue
        data=p.read_bytes()
        if name=='public-web/config.inc.php':
            for key in (b'password',b'admin_key',b'relay_control_key'):
                setting=re.search(rb"['\"]"+key+rb"['\"]\s*=>\s*(['\"])(.*?)\1",data)
                if setting is None or setting.group(2):
                    raise ReleaseError('Keep real service credentials in the deployed config.inc.php, not source control')
        if any(re.search(pattern,data) for pattern in PATTERNS):raise ReleaseError('Credential-like content in release input: '+name)
