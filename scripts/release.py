#!/usr/bin/env python3
"""Build, sign, verify and publish the same KeeperFX-E bytes. Adapted from Beam68 (MIT)."""
import argparse
from datetime import datetime, timezone
import ftplib
import hashlib
import importlib.util
import json
import os
import re
import shlex
import shutil
import subprocess
import sys
import struct
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path
from release_support import ReleaseError, atomic_json, canonical, fingerprint, sha, source_files

ROOT = Path(__file__).resolve().parents[1]
ALLOWED_ENV = {'KFX_SIGN_IDENTITY','KFX_NOTARY_PROFILE','KFX_GAME_DIR','KFX_MAC_DEPS_ROOT','KFX_JOBS','KFX_RELEASE_PYTHON'}
SECRET_ENV = ('GITHUB_TOKEN', 'GITLAB_TOKEN')

def need(condition, message):
    if not condition:
        raise ReleaseError(message)

def changelog_notes(root, version):
    """Render only one version's changes; the host displays the release title."""
    changelog = (root/'CHANGELOG.md').read_text()
    match = re.search(r'^## '+re.escape(version)+r'(?:[^\n]*)\n(.*?)(?=^## |\Z)',
                      changelog, re.M | re.S)
    need(match is not None, 'Missing version in CHANGELOG.md: '+version)
    return re.sub(r'^### ', '## ', match.group(1).strip(), flags=re.M)


def validate_release_notes(root, version, text):
    """Repository release descriptions contain the complete changelog only."""
    need(not re.search(r'\b(in development|unreleased|TODO|TBD)\b', text, re.I),
         'Release notes contain development placeholders')
    need(' '.join(text.split()) == ' '.join(changelog_notes(root, version).split()),
         'Release notes must contain only the complete matching CHANGELOG.md section')


def load(path):
    need(path.is_file() and not path.is_symlink(), 'Missing or linked metadata: ' + path.name)
    return json.loads(path.read_text())

def module(root, name):
    spec = importlib.util.spec_from_file_location(name.replace('-', '_'), root / 'scripts' / (name + '.py'))
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result

def repo_url(url, github=False):
    p = urllib.parse.urlsplit(url)
    need(p.scheme == 'https' and p.hostname and p.netloc == p.hostname and
         not p.username and not p.password and not p.query and not p.fragment,
         'Repository must be a credential-free HTTPS URL without a port/query')
    need(re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9.-]*', p.hostname) and
         (not github or p.hostname == 'github.com'), 'Unexpected repository host')
    parts = p.path.strip('/').split('/')
    need(len(parts) >= 2 and (not github or len(parts) == 2) and
         all(re.fullmatch(r'[A-Za-z0-9_][A-Za-z0-9_.-]*', x) and x not in ('.', '..') for x in parts),
         'Invalid repository path')
    need(not parts[-1].endswith('.git') and p.path == '/' + '/'.join(parts), 'Use the canonical repository web URL')
    return p.hostname, '/'.join(parts)

def git_url(url, web):
    host, path = repo_url(web, web.startswith('https://github.com/'))
    need(url in ('git@' + host + ':' + path + '.git', web + '.git'),
         'Git push destination does not match repository URL')
    return url

def config_read(root, path):
    defaults = {'github': '',
                'github_git': '',
                'gitlab': '', 'gitlab_git': '', 'author': 'KeeperFX-E contributors',
                'env': {}, 'credentials': {}, 'source_branch': ''}
    if path.exists():
        raw = load(path)
        need(isinstance(raw, dict) and not set(raw) - set(defaults), 'Unknown release configuration key')
        defaults.update(raw)
    c = defaults
    need(isinstance(c['env'], dict) and not set(c['env']) - ALLOWED_ENV and
         all(isinstance(v, str) and '\0' not in v and '\n' not in v for v in c['env'].values()),
         'Only documented non-secret build environment settings are allowed')
    for key in ('github', 'github_git', 'gitlab', 'gitlab_git', 'author', 'source_branch'):
        need(isinstance(c[key], str) and not any(ch in c[key] for ch in ('\r', '\n', '\0')), 'Invalid configuration value: ' + key)
    for key in ('github', 'gitlab'):
        if c[key]:
            repo_url(c[key], key == 'github')
            git_url(c[key + '_git'], c[key])
        else:
            need(not c[key + '_git'], 'A Git destination requires its repository web URL')
    need(isinstance(c['credentials'], dict) and set(c['credentials']) <= {'github','gitlab'}, 'Invalid credential service')
    for service, spec in c['credentials'].items():
        need(isinstance(spec, dict) and set(spec) <= {'provider','variable','reference','vault','item','field','account'}, 'Invalid credential configuration')
        need(all(isinstance(v,str) and not any(ch in v for ch in ('\r','\n','\0')) for v in spec.values()), 'Invalid credential selector')
        need(spec.get('provider','env') in ('env','1password'), 'Unknown credential provider')
        if spec.get('provider','env') == 'env':
            need(re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*',spec.get('variable',service.upper()+'_TOKEN')), 'Invalid token variable')
        else:
            need((spec.get('reference','').startswith('op://') and not any(spec.get(k) for k in ('vault','item','field'))) or
                 (not spec.get('reference') and all(spec.get(k) for k in ('vault','item','field'))), 'Use one exact 1Password reference or vault/item/field')
    return c

ACTIVE_SECRETS = set()

def credential(service, config):
    spec = config.get('credentials',{}).get(service,{})
    if spec.get('provider','env') == 'env':
        token = os.environ.get(spec.get('variable',service.upper()+'_TOKEN'),'')
    else:
        reference = spec.get('reference')
        command = ['op','read','--no-newline',reference] if reference else [
            'op','item','get',spec['item'],'--vault',spec['vault'],'--fields','label='+spec['field'],'--format','json','--reveal']
        if spec.get('account'):
            command += ['--account',spec['account']]
        result = subprocess.run(command,capture_output=True,text=True)
        need(result.returncode == 0, '1Password lookup failed or was cancelled for '+service+'; no secret output displayed')
        if reference:
            token = result.stdout
        else:
            try:
                data=json.loads(result.stdout)
                fields=data if isinstance(data,list) else data.get('fields',[data])
                matches=[f['value'] for f in fields if f.get('label')==spec['field'] and f.get('value')]
                need(len(matches)==1,'1Password field missing or ambiguous; use an exact secret reference')
                token=matches[0]
            except (ValueError,TypeError,KeyError,AttributeError):
                raise ReleaseError('1Password field could not be resolved; no secret output displayed') from None
    need(isinstance(token,str) and token.strip() and not token.strip().startswith('op://') and not any(c in token for c in ('\r','\n','\0')), 'Missing/unresolved token for '+service)
    token=token.strip()
    ACTIVE_SECRETS.add(token)
    return token

def release_tag(value, version):
    tag=version if value is None else value
    need(isinstance(tag,str) and re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*',tag) and '..' not in tag and not tag.endswith(('.','.lock')), 'Invalid release tag (use a simple name such as v0.1 or 0.1)')
    return tag

def redact(text):
    secrets = ACTIVE_SECRETS | {value for key,value in os.environ.items()
        if key.upper().endswith(('_TOKEN','_PASSWORD','_SECRET')) and value}
    for token in sorted(secrets,key=len,reverse=True):
        text=text.replace(token,'[redacted]')
    return text

def run(root, args, env=None, *, log_dir=None):
    clean = dict(os.environ)
    for key in list(clean):
        if key in SECRET_ENV or key.upper().endswith(('_TOKEN','_PASSWORD','_SECRET')):
            clean.pop(key, None)
    clean.update(env or {})
    verbose=clean.pop('KFX_RELEASE_VERBOSE','0')=='1'
    log_dir=Path(log_dir) if log_dir is not None else root/'outputs/release'
    is_build=args[0]!='git' and log_dir.is_dir()
    log=log_dir/'build.log'
    if is_build:
        need(not log.is_symlink(), 'Linked build log')
        with log.open('a') as stream:
            stream.write('\n$ '+redact(shlex.join([str(x) for x in args]))+'\n')
    if verbose and is_build:
        output=[]
        with log.open('a') as stream:
            process=subprocess.Popen(args,cwd=root,env=clean,stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT,text=True,errors='replace',bufsize=1)
            for line in process.stdout:
                output.append(line)
                safe=redact(line)
                stream.write(safe);stream.flush()
                print(safe,end='',flush=True)
            code=process.wait()
        text=''.join(output)
    else:
        result=subprocess.run(args,cwd=root,env=clean,capture_output=True)
        code=result.returncode
        text=result.stdout.decode(errors='replace')
        if is_build:
            with log.open('a') as stream:
                stream.write(redact(text+result.stderr.decode(errors='replace')))
    # Git helper output is never logged: it may contain transport credentials.
    if code and is_build:
        failure=log_dir/'failed-build.log'
        need(not failure.is_symlink(), 'Linked build failure log')
        failure.write_text(redact(text if verbose else text+result.stderr.decode(errors='replace')))
    need(code==0, 'Command failed: '+Path(args[0]).name+
         (' (see '+str(log)+')' if is_build else ' (Git diagnostics withheld)'))
    return text.strip()

def git(root, *args):
    return run(root, ['git', *args], {'GIT_TERMINAL_PROMPT': '0'})

def commit(root, required=False):
    try:
        top = git(root, 'rev-parse', '--show-toplevel')
        need(Path(top).samefile(root), 'Release root must be the Git checkout root')
        value = git(root, 'rev-parse', 'HEAD')
        need(re.fullmatch('[0-9a-f]{40,64}', value), 'Invalid source commit')
        return value
    except ReleaseError:
        if required:
            raise ReleaseError('Publication requires a Git checkout at the project root')
        return None

def tracked_source(root, files):
    tracked = set(git(root, 'ls-files', '-z').split('\0'))
    need(set(files) <= tracked, 'Release inputs must all be tracked; commit source changes first')

def clean_checkout(root, files):
    value = commit(root, True)
    need(not git(root, 'status', '--porcelain', '--untracked-files=all'), 'Publication requires a clean committed checkout')
    tracked_source(root, files)
    return value

def source_destination(config, service):
    need(service in ('github','gitlab') and config.get(service), 'Missing source repository for '+service)
    return git_url(config[service+'_git'],config[service])

def validate_tracked_public(root):
    for name in git(root, 'ls-files', '-z').split('\0'):
        if not name:
            continue
        path = Path(name)
        private = (any(part in {'.local', 'outputs', '__pycache__'} for part in path.parts)
                   or path.name.startswith('.env')
                   or path.suffix.lower() in {'.pem', '.key', '.p12', '.pfx'}
                   or path.name in {'.netrc', '.git-credentials', 'id_rsa', 'id_ed25519', 'credentials', 'release-config.json'})
        need(not private or name == 'examples/release-config.json', 'Private tracked source: '+name)


def initialize_source(config, services, root, message=None, dry_run=False):
    # Explicit --init may create local metadata, never an extra commit in an
    # established checkout and never a replacement for existing remote history.
    detected=subprocess.run(['git','rev-parse','--show-toplevel'],cwd=root,capture_output=True,text=True)
    if detected.returncode==0:
        need(Path(detected.stdout.strip()).samefile(root), 'Refusing initialization inside another repository')
        if commit(root):
            return
    else:
        need(not (root/'.git').exists() and not (root/'.git').is_symlink(), 'Inspect existing Git metadata before initialization')
    need((root/'.gitignore').is_file(), 'Review .gitignore before initialization')
    destinations={service:source_destination(config,service) for service in services}
    for destination in destinations.values():
        need(not git(root,'ls-remote','--refs',destination), 'Remote already has history; clone it and import source instead')
    for identity in ('GIT_AUTHOR_IDENT','GIT_COMMITTER_IDENT'):
        git(root,'var',identity)
    branch=config.get('source_branch') or 'main'
    git(root,'check-ref-format','--branch',branch)
    # A temporary index respects .gitignore without changing the source checkout.
    with tempfile.TemporaryDirectory(prefix='keeperfx-e-source-') as temporary:
        metadata = Path(temporary)/'repository.git'
        subprocess.run(['git', 'init', '--bare', '--quiet', str(metadata)], check=True, capture_output=True)
        env = dict(os.environ, GIT_DIR=str(metadata), GIT_WORK_TREE=str(root))
        from source_policy import excluded
        inventory = {name: data for name, data in source_files(root, env).items() if not excluded(name)}
    module(root,'check-sensitive').scan(root, inventory)
    if dry_run:
        print('Initialization preflight passed: '+str(len(inventory))+' public inputs. No checkout changes or upload performed.')
        return True
    if detected.returncode:
        git(root,'init','--initial-branch',branch)
    for service,destination in destinations.items():
        name='origin' if service=='gitlab' else 'github'
        try:
            existing=git(root,'remote','get-url',name)
        except ReleaseError:
            existing=None
        if existing:
            need(existing==destination, 'Existing source remote differs')
        else:
            git(root,'remote','add',name,destination)
    # Inspect public inputs before staging anything, including local/private files.
    module(root,'check-sensitive').scan(root)
    git(root,'add','--',*sorted(inventory))
    validate_tracked_public(root)
    git(root,'commit','-m',message or 'Initial KeeperFX-E source')
    print('Created the first local commit. No source uploaded.')

def push_source(config, services, confirmed=False, dry_run=False, root=ROOT, initialize=False, message=None):
    need(services and len(services)==len(set(services)) and set(services)<={'github','gitlab'}, 'Source supports distinct github/gitlab destinations only')
    need(dry_run or confirmed, 'Source upload requires --confirm-push; use --dry-run first')
    destinations={service:source_destination(config,service) for service in services}
    if initialize:
        if initialize_source(config,services,root,message,dry_run):
            return
    validate_tracked_public(root)
    from source_policy import validate_history
    validate_history(root, git)
    module(root,'check-sensitive').scan(root)
    head=clean_checkout(root,source_files(root))
    branch=git(root,'symbolic-ref','--quiet','--short','HEAD')
    target=config.get('source_branch') or branch
    git(root,'check-ref-format','--branch',target)
    ref=head+':refs/heads/'+target
    for destination in destinations.values():
        git(root,'push','--dry-run','--no-follow-tags',destination,ref)
    if dry_run:
        print('Source preflight passed. No upload performed.')
        return
    for service,destination in destinations.items():
        need(clean_checkout(root,source_files(root))==head,'Source changed during push')
        git(root,'push','--no-follow-tags',destination,ref)
        remote=git(root,'ls-remote','--heads',destination,'refs/heads/'+target)
        need(remote and remote.split()[0]==head, 'Remote source branch verification failed')
        print(service+': committed source verified')

class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None

class API:
    """HTTPS only; API calls never forward authorization through redirects."""
    def __init__(self, origin, token, github=False):
        self.origin = origin
        self.installation_token = github and token.startswith('ghs_')
        self.headers = {'User-Agent': 'KeeperFX-E-release', 'Accept': 'application/json'}
        if github:
            self.headers.update({'Authorization': 'Bearer ' + token, 'Accept': 'application/vnd.github+json',
                                 'X-GitHub-Api-Version': '2022-11-28'})
        else:
            self.headers['PRIVATE-TOKEN'] = token
        self.opener = urllib.request.build_opener(NoRedirect())

    def open(self, url, method='GET', data=None, headers=None, missing=False):
        p = urllib.parse.urlsplit(url)
        need(p.scheme == 'https' and not p.username and not p.password, 'Invalid API destination')
        h = dict(self.headers)
        allowed = {self.origin}
        if self.origin == 'https://api.github.com':
            allowed.add('https://uploads.github.com')
        need(p.scheme + '://' + p.netloc in allowed, 'API origin mismatch')
        h.update(headers or {})
        try:
            return self.opener.open(urllib.request.Request(url, data=data, headers=h, method=method), timeout=60)
        except urllib.error.HTTPError as exc:
            if missing and exc.code == 404:
                return None
            raise ReleaseError('Release service returned HTTP ' + str(exc.code)) from None
        except (urllib.error.URLError, OSError):
            raise ReleaseError('Release service connection failed') from None

    def json(self, url, method='GET', data=None, file=None, missing=False):
        headers = {}
        if file:
            with file.open('rb') as stream:
                response = self.open(url, method, stream, {'Content-Type': 'application/octet-stream',
                                                         'Content-Length': str(file.stat().st_size)}, missing)
        else:
            body = canonical(data) if data is not None else None
            if body is not None:
                headers['Content-Type'] = 'application/json'
            response = self.open(url, method, body, headers, missing)
        if response is None:
            return None
        with response:
            raw = response.read(8 * 1024 * 1024 + 1)
        need(len(raw) <= 8 * 1024 * 1024, 'Oversized API response')
        return json.loads(raw)

    def download_hash(self, url, size):
        # Generic packages may redirect to object storage. Never forward tokens there.
        headers = dict(self.headers)
        for attempt in range(6):
            p = urllib.parse.urlsplit(url)
            need(p.hostname and p.netloc == p.hostname and p.path, 'Invalid package URL')
            need(p.scheme == 'https' and not p.username and not p.password, 'Unsafe package redirect')
            if p.scheme + '://' + p.netloc != self.origin:
                headers = {'User-Agent': 'KeeperFX-E-release'}
            try:
                response = self.opener.open(urllib.request.Request(url, headers=headers), timeout=60)
            except urllib.error.HTTPError as exc:
                if exc.code == 404 and attempt == 0:
                    return None
                if exc.code in (301, 302, 303, 307, 308):
                    location = exc.headers.get('Location')
                    need(location, 'Package redirect is missing its destination')
                    url = urllib.parse.urljoin(url, location)
                    continue
                raise ReleaseError('Package download returned HTTP ' + str(exc.code)) from None
            except (urllib.error.URLError, OSError):
                raise ReleaseError('Package verification connection failed') from None
            h = hashlib.sha256()
            total = 0
            with response:
                for chunk in iter(lambda: response.read(1024 * 1024), b''):
                    total += len(chunk)
                    need(total <= size, 'Remote package size conflicts with release')
                    h.update(chunk)
            need(total == size, 'Remote package is truncated')
            return h.hexdigest()
        raise ReleaseError('Too many package redirects')

    def pages(self, url):
        result = []
        for page in range(1, 1001):
            items = self.json(url + ('&' if '?' in url else '?') + 'per_page=100&page=' + str(page))
            need(isinstance(items, list), 'Invalid paginated response')
            result.extend(items)
            if len(items) < 100:
                return result
        raise ReleaseError('Release service pagination limit exceeded')

class GitHub:
    def __init__(self, manifest, directory, api=None, token=None):
        self.m, self.directory = manifest, directory
        host, path = repo_url(manifest['github'], True)
        self.base = 'https://api.github.com/repos/' + path
        self.upload = 'https://uploads.github.com/repos/' + path
        self.api = api or API('https://api.github.com', token or os.environ['GITHUB_TOKEN'], True)

    def release(self):
        releases = self.api.pages(self.base + '/releases')
        found = [x for x in releases if x['tag_name'] == self.m['tag']]
        need(len(found) <= 1, 'Duplicate GitHub releases')
        return found[0] if found else None

    def assets(self, release):
        ident = str(release['id'])
        need(ident.isdigit(), 'Invalid GitHub release ID')
        return self.api.pages(self.base + '/releases/' + ident + '/assets')

    def verify(self, release, complete=False):
        need((release.get('body') or '') == self.m['notes'], 'GitHub release notes conflict')
        assets = self.assets(release)
        names = [a['name'] for a in assets]
        need(len(names) == len(set(names)) and set(names) <= set(self.m['files']), 'Unexpected GitHub release assets')
        for asset in assets:
            name = asset['name']
            need(asset.get('state') == 'uploaded' and asset.get('digest') == 'sha256:' + self.m['files'][name] and
                 asset.get('size') == (self.directory / name).stat().st_size, 'Unknown or conflicting GitHub asset: ' + name)
        if complete or not release['draft']:
            need(set(names) == set(self.m['files']), 'Published GitHub release has an incomplete asset inventory')
        return set(names)

    def preflight(self):
        repo = self.api.json(self.base)
        need(repo.get('html_url') == self.m['github'], 'GitHub repository identity mismatch; check the configured repository URL')
        need(not repo.get('archived') and not repo.get('disabled'), 'GitHub repository is archived or disabled')
        if getattr(self.api,'installation_token',False):
            # Installation tokens act as an App, not as a repository user.
            # The user-role permissions.push value is not their Contents grant.
            found=False
            for page in range(1,1001):
                listing=self.api.json('https://api.github.com/installation/repositories?per_page=100&page='+str(page))
                entries=listing.get('repositories') if isinstance(listing,dict) else None
                need(isinstance(entries,list),'Invalid GitHub installation repository response')
                for entry in entries:
                    if entry.get('id')==repo.get('id') and repo.get('id') is not None and entry.get('html_url')==self.m['github']:
                        found=True
                if found or len(entries)<100:
                    break
            need(found,'GitHub App token does not include the configured repository')
            # This read-only check proves installation access. GitHub enforces
            # Contents:write on release creation/upload; it is not a user role.
        else:
            permissions=repo.get('permissions')
            need(isinstance(permissions,dict) and 'push' in permissions,
                 'GitHub did not report repository write access for this credential; check the configured token and repository access')
            need(permissions['push'] is True,
                 'GitHub reports push=false for the configured user credential. Check its account/resource owner, selected repository, Contents read/write permission and organization approval in the configured credential store')
        release = self.release()
        if release:
            self.verify(release)

    def publish(self):
        release = self.release()
        if release is None:
            release = self.api.json(self.base + '/releases', 'POST',
                                    {'tag_name': self.m['tag'], 'target_commitish': self.m['commit'],
                                     'name': 'KeeperFX-E ' + self.m['version'], 'body': self.m['notes'],
                                     'draft': True, 'prerelease': False})
        existing = self.verify(release)
        ident = str(release['id'])
        for name, digest in self.m['files'].items():
            if name not in existing:
                asset = self.api.json(self.upload + '/releases/' + ident + '/assets?name=' + urllib.parse.quote(name),
                                      'POST', file=self.directory / name)
                need(asset.get('digest') == 'sha256:' + digest and asset.get('state') == 'uploaded' and
                     asset.get('size') == (self.directory / name).stat().st_size,
                     'GitHub did not verify uploaded asset: ' + name)
        self.verify(release, True)
        self.api.json(self.base + '/releases/' + ident, 'PATCH',
                      {'name': 'KeeperFX-E ' + self.m['version'], 'draft': False, 'prerelease': False, 'make_latest': 'true'})
        final = self.release()
        need(final and not final['draft'] and final.get('prerelease') is False,
             'GitHub release is not a normal published release')
        self.verify(final, True)
        latest = self.api.json(self.base + '/releases/latest')
        need(latest and latest.get('tag_name') == self.m['tag'], 'GitHub release is not latest')

class GitLab:
    def __init__(self, manifest, directory, api=None, token=None):
        self.m, self.directory = manifest, directory
        host, path = repo_url(manifest['gitlab'])
        self.origin = 'https://' + host
        self.base = self.origin + '/api/v4/projects/' + urllib.parse.quote(path, safe='')
        self.api = api or API(self.origin, token or os.environ['GITLAB_TOKEN'])

    def package_url(self, name):
        return self.base + '/packages/generic/keeperfx-e/' + urllib.parse.quote(self.m['version'], safe='') + '/' + urllib.parse.quote(name, safe='')

    def links(self):
        return [{'name': n, 'url': self.package_url(n), 'link_type': 'package'} for n in self.m['files']]

    def release(self):
        return self.api.json(self.base + '/releases/' + urllib.parse.quote(self.m['tag'], safe=''), missing=True)

    def verify(self, release):
        need(release.get('description') == self.m['notes'], 'GitLab release notes conflict')
        links = release.get('assets', {}).get('links', [])
        actual = [(x['name'], x['url']) for x in links]
        need(len(actual) == len(set(actual)) and sorted(actual) == sorted((x['name'], x['url']) for x in self.links()), 'GitLab release links conflict')

    def packages(self, require_all=False):
        existing = set()
        for name, digest in self.m['files'].items():
            got = self.api.download_hash(self.package_url(name), (self.directory / name).stat().st_size)
            if got is not None:
                need(got == digest, 'GitLab package bytes conflict: ' + name)
                existing.add(name)
        if require_all:
            need(existing == set(self.m['files']), 'Missing GitLab packages')
        return existing

    def preflight(self):
        project = self.api.json(self.base)
        permissions = project.get('permissions', {})
        level = max((v or {}).get('access_level', 0) for v in
                    (permissions.get('project_access'), permissions.get('group_access')))
        need(project.get('web_url') == self.m['gitlab'], 'GitLab repository identity mismatch; check the configured repository URL')
        need(not project.get('archived'), 'GitLab project is archived')
        need(project.get('packages_enabled') is not False, 'GitLab package registry is disabled')
        need(level >= 30, 'GitLab requires Developer access (30) or higher for releases and generic packages; configured credential has access level '+str(level))
        self.packages()
        release = self.release()
        if release:
            self.verify(release)

    def publish(self):
        existing = self.packages()
        for name in self.m['files']:
            if name not in existing:
                self.api.json(self.package_url(name), 'PUT', file=self.directory / name)
                need(self.api.download_hash(self.package_url(name), (self.directory / name).stat().st_size) == self.m['files'][name], 'GitLab upload verification failed: ' + name)
        self.packages(True)
        release = self.release()
        released_at = datetime.now(timezone.utc).isoformat(timespec='seconds')
        if release:
            self.verify(release)
            self.api.json(self.base + '/releases/' + urllib.parse.quote(self.m['tag'], safe=''),
                          'PUT', {'name': 'KeeperFX-E ' + self.m['version'], 'released_at': released_at})
        else:
            self.api.json(self.base + '/releases', 'POST', {'tag_name': self.m['tag'],
                          'name': 'KeeperFX-E ' + self.m['version'], 'description': self.m['notes'],
                          'released_at': released_at, 'assets': {'links': self.links()}})
        final = self.release()
        self.verify(final)
        need(not final.get('upcoming_release', False), 'GitLab release is still upcoming')
        latest = self.api.json(self.base + '/releases/permalink/latest')
        need(latest and latest.get('tag_name') == self.m['tag'], 'GitLab release is not latest')

def normalized_macho(data):
    """Normalize only signing's leftover __LINKEDIT virtual allocation size."""
    blob=bytearray(data)
    need(len(blob)>=32 and struct.unpack_from('<I',blob)[0]==0xfeedfacf,'Expected thin 64-bit Mach-O')
    count,size=struct.unpack_from('<II',blob,16);need(32+size<=len(blob),'Truncated Mach-O commands')
    offset=32
    for _ in range(count):
        need(offset+8<=32+size,'Truncated Mach-O command');cmd,length=struct.unpack_from('<II',blob,offset)
        need(length>=8 and offset+length<=32+size,'Invalid Mach-O command')
        if cmd==0x19:
            need(length>=72,'Truncated segment command')
            if blob[offset+8:offset+24].rstrip(b'\0')==b'__LINKEDIT':struct.pack_into('<Q',blob,offset+32,0)
        offset+=length
    return bytes(blob)

def executable_code_hashes(path):
    hashes={}
    with tempfile.TemporaryDirectory(prefix='kfx-unsigned-code-') as folder:
        for arch in ('arm64','x86_64'):
            dest=Path(folder)/arch
            run(ROOT,['lipo',str(path),'-thin',arch,'-output',str(dest)])
            run(ROOT,['codesign','--remove-signature',str(dest)])
            hashes[arch]=hashlib.sha256(normalized_macho(dest.read_bytes())).hexdigest()
    return hashes

class Release:
    def __init__(self,root,config,tag=None,tag_message=None,output=None):
        self.root,self.config=root,config
        version=(root/'VERSION').read_text().strip()
        need(re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+',version),'VERSION must use major.minor.patch')
        self.version,self.tag=version,release_tag(tag,version)
        self.tag_message=tag_message or 'KeeperFX-E '+self.tag
        need(not any(c in self.tag_message for c in ('\0','\r','\n')),'Invalid tag annotation')
        self.out=Path(os.path.abspath(root/(output or ('outputs/release-'+version))))
        base=root/'outputs';need(base in self.out.parents,'Output must be beneath outputs/')
        p=self.out
        while p!=root:
            need(not p.is_symlink(),'Linked release directory');p=p.parent
        self.state=self.out/'release';self.record=self.state/'build.json';self.manifest=self.state/'manifest.json'
        self.payload_names=['KeeperFX-E-'+version+'-macOS-Universal.'+ext for ext in ('dmg','zip')]
        self.names=sorted(self.payload_names+[n+'.sha256' for n in self.payload_names])
        self.verbose=False;self.allow_dirty=False
    def inputs(self):return source_files(self.root)
    def binding(self):return {key:self.config[key] for key in ('github','github_git','gitlab','gitlab_git','author')}
    def build_binding(self):return hashlib.sha256(canonical(self.config['env'])).hexdigest()
    def plan(self):
        py=self.config['env'].get('KFX_RELEASE_PYTHON',sys.executable)
        return [[py,'macos/tests/test-release-tool.py'],['zsh','macos/build-universal.sh'],
                [py,'macos/tests/run-ingame-options-tests.py'],[py,'macos/tests/run-controller-tests.py'],[py,'macos/tests/run-frame-wait-tests.py'],[py,'macos/tests/check-play-tools.py'],[py,'macos/tests/check-network-resume.py'],[py,'macos/tests/test-connectivity.py'],
                [py,'macos/tests/run-readiness-tests.py'],[py,'macos/tests/run-hub-config-tests.py'],[py,'macos/tests/test-native-php-api.py'],
                [py,'macos/tests/test-php-web.py'],[py,'macos/tests/test-web-deploy.py'],[py,'macos/tests/test-service-operations.py'],
                ['zsh','macos/package-app.sh']]
    def app(self):return self.state/'package/KeeperFX-E.app'
    def verify_package(self):
        app=self.app();need(app.is_dir() and not app.is_symlink(),'Missing app package')
        import plistlib
        info=plistlib.loads((app/'Contents/Info.plist').read_bytes())
        need(info['CFBundleDisplayName']=='KeeperFX-E' and info['CFBundleShortVersionString']==self.version and info['CFBundleVersion']==(self.root/'macos/BUILD_NUMBER').read_text().strip(),'App version/name mismatch')
        executables=['Contents/MacOS/KeeperFXE','Contents/Resources/game/keeperfx','Contents/Resources/connection-check','Contents/Resources/controller-check','Contents/Resources/video-check']
        for name in executables:
            p=app/name;need(p.is_file() and not p.is_symlink(),'Missing packaged executable')
            need(set(run(self.root,['lipo','-archs',str(p)]).split())=={'arm64','x86_64'},'Package is not universal')
            linked=run(self.root,['otool','-L',str(p)])
            need('/opt/homebrew' not in linked and '/usr/local' not in linked,'External runtime library in package')
        need(executable_code_hashes(app/'Contents/Resources/game/keeperfx')==executable_code_hashes(self.root/'build-mac-universal/keeperfx'),'Packaged executable code differs from built engine')
        game=app/'Contents/Resources/game'
        for folder,names in {'data':['bluepal.dat','bluepall.dat','dogpal.pal','hitpall.dat','lightng.pal','redpal.col','redpall.dat','slab0-0.dat','slab0-1.dat','vampal.pal','whitepal.col'],'sound':['atmos1.sbk','atmos2.sbk','bullfrog.sbk']}.items():
            need(not any((game/folder/n).exists() for n in names),'Original CD data included in package')
        need(not (game/'save').exists() and not list((game/'ldata').glob('*.smk')),'Personal saves or CD movies included')
        run(self.root,['codesign','--verify','--deep','--strict',str(app)])
        run(self.root,['xcrun','stapler','validate',str(app)])
        run(self.root,['spctl','--assess','--type','execute',str(app)])
        dmg=self.out/self.payload_names[0]
        run(self.root,['codesign','--verify',str(dmg)])
        run(self.root,['xcrun','stapler','validate',str(dmg)])
        run(self.root,['hdiutil','verify',str(dmg)])
        # Inspect the actual mounted DMG, rather than trusting its staging directory.
        mounted=None
        try:
            plist=run(self.root,['hdiutil','attach','-readonly','-nobrowse','-plist',str(dmg)])
            entities=plistlib.loads(plist.encode())['system-entities'];mounted=next(e['mount-point'] for e in entities if 'mount-point' in e)
            need(self.app_inventory(Path(mounted)/app.name)==self.app_inventory(app),'DMG app differs from signed app')
        finally:
            if mounted:run(self.root,['hdiutil','detach',mounted])
        # ZIP must unpack to exactly the same signed/stapled app as the DMG stage.
        with tempfile.TemporaryDirectory(prefix='kfx-release-check-') as temp:
            run(self.root,['ditto','-x','-k',str(self.out/self.payload_names[1]),temp])
            need(self.app_inventory(Path(temp)/app.name)==self.app_inventory(app),'ZIP app differs from signed app')
    @staticmethod
    def app_inventory(app):
        result={}
        for p in sorted(app.rglob('*')):
            if p.is_symlink():result[str(p.relative_to(app))]={'link':os.readlink(p)}
            elif p.is_file():result[str(p.relative_to(app))]={'sha256':sha(p),'mode':p.stat().st_mode&0o777}
        return result
    def build(self):
        need(not self.out.exists(),'Use a fresh version-specific output directory; existing builds are preserved')
        module(self.root,'check-sensitive').scan(self.root)
        before=self.inputs();head=commit(self.root)
        if not self.allow_dirty:clean_checkout(self.root,before)
        self.state.mkdir(parents=True);log=self.state/'build.log';log.write_text('KeeperFX-E '+self.version+'\n')
        env=dict(self.config['env']);need(env.get('KFX_SIGN_IDENTITY') and env.get('KFX_NOTARY_PROFILE'),'Signing identity and saved notary profile are required')
        env['KFX_PACKAGE_DIR']=str(self.state/'package');env['KFX_RELEASE_VERBOSE']='1' if self.verbose else '0'
        for index,args in enumerate(self.plan(),1):
            print('[%d/%d] %s'%(index,len(self.plan()),shlex.join(args)),flush=True);run(self.root,args,env,log_dir=self.state)
        need(self.inputs()==before,'Release inputs changed during build; use a fresh build')
        for name in self.payload_names:
            source=self.state/'package'/name;need(source.is_file(),'Package did not produce '+name);shutil.copyfile(source,self.out/name)
            (self.out/(name+'.sha256')).write_text(sha(self.out/name)+'  '+name+'\n')
        self.verify_package()
        provenance={'schema':1,'product':'KeeperFX-E','version':self.version,'app_build':(self.root/'macos/BUILD_NUMBER').read_text().strip(),
                    'commit':head,'dirty':bool(git(self.root,'status','--porcelain','--untracked-files=all')),'source_sha256':fingerprint(before),
                    'inputs':before,'configuration_sha256':self.build_binding(),'target':{'architectures':['arm64','x86_64'],'minimum_macos':'13.0'},
                    'compiler':run(self.root,['clang','--version']).splitlines()[0],
                    'engine_sha256':sha(self.root/'build-mac-universal/keeperfx'),'app_inventory':self.app_inventory(self.app()),
                    'artifacts':{n:sha(self.out/n) for n in self.names}}
        atomic_json(self.record,provenance);self.check(verify_native=False)
        print('Build verified. No publication performed. '+str(self.out),flush=True)
    def check(self,verify_native=True):
        module(self.root,'check-sensitive').scan(self.root);record=load(self.record)
        need(record.get('inputs')==self.inputs() and record.get('source_sha256')==fingerprint(self.inputs()),'Stale source fingerprint; rebuild')
        need(record.get('version')==self.version and record.get('configuration_sha256')==self.build_binding(),'Build configuration changed')
        need({p.name for p in self.out.iterdir()}==set(self.names)|{'release'},'Unexpected release inventory')
        for n in self.names:need((self.out/n).is_file() and not (self.out/n).is_symlink(),'Missing or linked artifact')
        need(record.get('artifacts')=={n:sha(self.out/n) for n in self.names},'Artifact checksum changed')
        for n in self.payload_names:need((self.out/(n+'.sha256')).read_text()==sha(self.out/n)+'  '+n+'\n','Changed checksum file')
        need(record.get('app_inventory')==self.app_inventory(self.app()),'Signed app changed')
        if verify_native:self.verify_package()
        return {n:sha(self.out/n) for n in self.names}
    def retire_obsolete(self,folder,ignore=()):
        need({p.name for p in folder.iterdir()}-set(ignore)<=set(self.names),'Unexpected publication assets')
    def prepare(self, notes):
        files = self.check()
        text = notes.read_text().strip()
        need(text and len(text.encode()) <= 100000, 'Missing or oversized release notes')
        validate_release_notes(self.root, self.version, text)
        need(not any(token and token in text for token in (os.environ.get(x) for x in SECRET_ENV)), 'Release notes contain a credential')
        manifest = {'schema': 1, 'version': self.version, 'tag': self.tag, 'source_sha256': fingerprint(self.inputs()),
                    'tag_message': self.tag_message, 'commit': commit(self.root), 'build_sha256': sha(self.record), 'notes': text, 'files': files,
                    **self.binding()}
        if self.manifest.exists() and load(self.manifest) != manifest:
            need(not (self.state / 'receipt.json').exists(),
                 'Prepared manifest has publication receipts; do not replace it')
        stage=self.state/'publish'/'outputs'
        need(not stage.is_symlink() and not stage.parent.is_symlink(), 'Linked publication stage')
        if stage.exists():
            need(not stage.is_symlink() and not stage.parent.is_symlink(), 'Linked publication stage')
            self.retire_obsolete(stage)
        stage.mkdir(parents=True,exist_ok=True)
        for name in files:
            target=stage/name
            need(not target.is_symlink(), 'Linked prepared asset')
            # Never replace reviewed bytes once a receipt exists.
            if (self.state/'receipt.json').exists():
                need(target.is_file() and sha(target)==files[name], 'Receipted prepared asset changed')
            else:
                shutil.copyfile(self.out/name,target)
        self.prepared_check(manifest)
        atomic_json(self.manifest, manifest)
        return manifest

    def prepared_check(self, manifest):
        stage=self.state/'publish'/'outputs'
        need(not stage.is_symlink() and not stage.parent.is_symlink() and stage.is_dir(), 'Missing or linked publication stage')
        need({p.name for p in stage.iterdir()}==set(manifest['files']), 'Prepared publication inventory differs')
        for name,digest in manifest['files'].items():
            path=stage/name
            need(path.is_file() and not path.is_symlink() and sha(path)==digest, 'Prepared asset changed: '+name)
        return stage

    def manifest_check(self):
        m = load(self.manifest)
        need(m.get('files') == self.check() and m.get('build_sha256') == sha(self.record) and
             m.get('source_sha256') == fingerprint(self.inputs()) and m.get('version') == self.version and
             m.get('tag') == self.tag and m.get('tag_message') == self.tag_message, 'Prepared manifest is stale; prepare again')
        need(all(m.get(k) == v for k, v in self.binding().items()), 'Prepared destinations changed')
        need(m.get('commit') == clean_checkout(self.root, self.inputs()), 'Prepared commit differs; prepare again')
        need(isinstance(m.get('notes'), str) and m['notes'], 'Missing prepared notes')
        validate_release_notes(self.root, self.version, m['notes'])
        self.prepared_check(m)
        return m

    def tag_preflight(self, m, remote):
        try:
            local = git(self.root, 'rev-parse', '--verify', 'refs/tags/' + m['tag'])
        except ReleaseError:
            local = None
        if local:
            need(git(self.root, 'cat-file', '-t', 'refs/tags/' + m['tag']) == 'tag', 'Existing local release tag must be annotated')
            need(git(self.root, 'rev-parse', 'refs/tags/' + m['tag'] + '^{commit}') == m['commit'], 'Local tag points to a different commit')
        lines = git(self.root, 'ls-remote', '--tags', remote, 'refs/tags/' + m['tag'], 'refs/tags/' + m['tag'] + '^{}').splitlines()
        refs = dict(line.split('\t')[::-1] for line in lines if line)
        peeled = refs.get('refs/tags/' + m['tag'] + '^{}')
        direct = refs.get('refs/tags/' + m['tag'])
        if direct:
            need(peeled == m['commit'], 'Remote tag differs or is not annotated')
            if local:
                need(direct == local, 'Remote annotated tag object differs from local tag')
        return direct

    def push_tag(self, m, remote):
        from source_policy import validate_history
        validate_history(self.root, git)
        existing = self.tag_preflight(m, remote)
        if existing:
            # Fetch only this tag if no local tag exists. Never move an existing tag.
            try:
                git(self.root, 'rev-parse', '--verify', 'refs/tags/' + m['tag'])
            except ReleaseError:
                git(self.root, 'fetch', '--no-tags', remote, 'refs/tags/' + m['tag'] + ':refs/tags/' + m['tag'])
        else:
            try:
                git(self.root, 'rev-parse', '--verify', 'refs/tags/' + m['tag'])
            except ReleaseError:
                git(self.root, '-c', 'tag.gpgSign=false', 'tag', '-a', m['tag'], m['commit'], '-m', m.get('tag_message','KeeperFX-E '+m['tag']))
            git(self.root, 'push', '--no-follow-tags', remote, 'refs/tags/' + m['tag'] + ':refs/tags/' + m['tag'])
        need(self.tag_preflight(m, remote), 'Remote tag verification failed')

    def publish(self,destinations,confirmed):
        from source_policy import validate_history
        validate_history(self.root, git)
        need(confirmed,'Publication requires --confirm-publish')
        need(destinations and len(destinations)==len(set(destinations)) and set(destinations)<={'github','gitlab'},'Select github and/or gitlab explicitly')
        m=self.manifest_check();need(not load(self.record).get('dirty'),'Review build cannot be published; commit inputs and rebuild')
        key=sha(self.manifest);receipt_path=self.state/'receipt.json'
        receipt=load(receipt_path) if receipt_path.exists() else {'manifest_sha256':key,'current':None,'completed':[]}
        need(receipt.get('manifest_sha256')==key,'Receipt belongs to another manifest')
        stage=self.prepared_check(m);tokens={};adapters={};tags=[]
        for target in destinations:
            need(m[target],'Configure the intended '+target+' repository');tokens[target]=credential(target,self.config)
        for target in destinations:
            direct=self.tag_preflight(m,m[target+'_git'])
            if direct:tags.append(direct)
            adapters[target]=(GitHub if target=='github' else GitLab)(m,stage,token=tokens[target]);adapters[target].preflight()
        need(len(set(tags))<=1,'Destinations contain different annotated tag objects');self.manifest_check()
        for target in destinations:
            if target in receipt['completed']:continue
            receipt['current']=target;atomic_json(receipt_path,receipt)
            for other in destinations:
                if self.tag_preflight(m,m[other+'_git']):self.push_tag(m,m[other+'_git']);break
            self.push_tag(m,m[target+'_git']);need(self.check()==m['files'],'Artifacts changed before upload');self.prepared_check(m)
            adapters[target].publish();receipt['completed'].append(target);receipt['current']=None;atomic_json(receipt_path,receipt)
        return receipt

def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('command',choices=['plan','source','build','check','prepare','publish','release'])
    p.add_argument('--config',type=Path,default=Path(os.environ.get('KFX_RELEASE_CONFIG',str(Path.home()/'Development/tools/git-release/projects/keeperfx-e.json'))))
    p.add_argument('--output',type=Path);p.add_argument('--notes',type=Path,default=ROOT/'docs/RELEASE-NOTES.md')
    p.add_argument('--to',default='');p.add_argument('--confirm-publish',action='store_true');p.add_argument('--confirm-push',action='store_true');p.add_argument('--dry-run',action='store_true')
    p.add_argument('--init',action='store_true',help='Initialize a new public source checkout')
    p.add_argument('--commit-message',help='Initial source commit message')
    p.add_argument('--tag');p.add_argument('--tag-message');p.add_argument('--verbose',action='store_true');p.add_argument('--allow-dirty',action='store_true',help='Local review builds only; publishing still requires clean committed source')
    a=p.parse_args(argv);tool=Release(ROOT,config_read(ROOT,a.config),a.tag,a.tag_message,a.output);tool.verbose=a.verbose;tool.allow_dirty=a.allow_dirty
    need(not a.allow_dirty or a.command=='build','--allow-dirty applies only to build')
    need(not a.init or a.command=='source','--init applies only to source')
    need(not a.commit_message or a.init,'--commit-message requires --init')
    if a.command=='plan':
        for command in tool.plan():print(shlex.join(command))
        print('Signed/notarized DMG and ZIP, individual SHA-256 files; no uploads.');return
    if a.command=='source':push_source(tool.config,a.to.split(','),a.confirm_push,a.dry_run,ROOT,a.init,a.commit_message);return
    if a.command=='build':tool.build()
    elif a.command=='check':tool.check()
    elif a.command=='prepare':tool.prepare(a.notes)
    else:
        need(a.confirm_publish,'Publication requires --confirm-publish')
        if a.command=='release':tool.prepare(a.notes)
        tool.publish(a.to.split(','),a.confirm_publish)
    print(a.command.capitalize()+' completed.')

if __name__=='__main__':
    try:main()
    except (ReleaseError,OSError,ValueError,KeyError,subprocess.SubprocessError) as error:
        message=str(error) if isinstance(error,ReleaseError) else 'Operation failed; inspect local logs and receipts.'
        print('Release refused: '+redact(message),file=sys.stderr);sys.exit(1)
