"""Offline macOS release acceptance checks; no real publication."""
import copy,hashlib,io,json,os,subprocess,sys,tempfile,unittest,struct
from pathlib import Path
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'scripts'))
import release as rel
from release_support import ReleaseError,atomic_json,sha,fingerprint
class GithubAPI:
    def __init__(self):
        self.r = None
        self.assets = []
        self.events = []
        self.bad_digest = False
        self.fail_upload = None
    def pages(self, url):
        return copy.deepcopy(self.assets if '/assets?' in url or url.endswith('/assets') else ([self.r] if self.r else []))
    def json(self, url, method='GET', data=None, file=None, missing=False):
        self.events.append((method, url))
        if method == 'GET':
            if url.endswith('/releases/latest'):
                return copy.deepcopy(self.r)
            return {'html_url': 'https://github.com/example/keeperfx-e', 'permissions': {'push': True}}
        if file:
            if self.fail_upload == file.name:
                raise ReleaseError('Injected interrupted upload')
            a = {'name': file.name, 'digest': None if self.bad_digest else 'sha256:' + sha(file),
                 'state': 'uploaded', 'size': file.stat().st_size}
            self.assets.append(a)
            return copy.deepcopy(a)
        if method == 'POST':
            self.r = dict(data, id=1)
            return copy.deepcopy(self.r)
        if method == 'PATCH':
            self.r.update(data)
            return copy.deepcopy(self.r)
        raise AssertionError((method, url))

class GitlabAPI:
    def __init__(self, url):
        self.url, self.r, self.files, self.events = url, None, {}, []
        self.corrupt = False
    def json(self, url, method='GET', data=None, file=None, missing=False):
        self.events.append((method, url))
        if method == 'PUT' and '/releases/' in url:
            self.r.update(data)
            self.r['upcoming_release'] = False
            return self.r
        if method == 'PUT':
            self.files[url] = file.read_bytes() + (b'bad' if self.corrupt else b'')
            return {'message': '201 Created'}
        if method == 'POST':
            self.r = data
            return data
        if '/releases/' in url:
            return self.r
        return {'web_url': self.url, 'permissions': {'group_access': {'access_level': 40}}}
    def download_hash(self, url, size):
        self.events.append(('VERIFY', url))
        data = self.files.get(url)
        return hashlib.sha256(data).hexdigest() if data is not None else None


class FixtureRelease(rel.Release):
    def verify_package(self):pass # Apple tools are covered separately by a real signed build.

class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='kfx-release-test-');self.addCleanup(self.temp.cleanup);self.root=Path(self.temp.name)
        for name in ['VERSION','macos/BUILD_NUMBER','LICENSE','CMakeLists.txt','src/main.cpp','scripts/release.py','macos/package-app.sh','README.md']:
            p=self.root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('fixture\n')
        (self.root/'scripts/check-sensitive.py').write_text((ROOT/'scripts/check-sensitive.py').read_text())
        (self.root/'VERSION').write_text('1.4.0\n');(self.root/'macos/BUILD_NUMBER').write_text('20261118.10\n');(self.root/'.gitignore').write_text('outputs/\n__pycache__/\n')
        (self.root/'CHANGELOG.md').write_text('# Changes\n\n## 1.4.0\n\n### Bugs\n- Fixed menus.\n');self.notes=self.root/'notes.md';self.notes.write_text('## Bugs\n- Fixed menus.\n')
        for args in [['init','-q'],['config','user.name','Fixture'],['config','user.email','fixture@example.invalid'],['add','.'],['commit','-qm','Fixture']]:subprocess.run(['git',*args],cwd=self.root,check=True,capture_output=True)
        self.config=rel.config_read(self.root,self.root/'missing-config.json');self.config.update(github='https://github.com/example/keeperfx-e',github_git='git@github.com:example/keeperfx-e.git')
        self.tool=FixtureRelease(self.root,self.config)
    def build(self):
        t=self.tool;t.state.mkdir(parents=True,exist_ok=True);t.app().mkdir(parents=True,exist_ok=True);(t.app()/'fixture').write_bytes(b'app')
        for n in t.payload_names:(t.out/n).write_bytes(n.encode());(t.out/(n+'.sha256')).write_text(sha(t.out/n)+'  '+n+'\n')
        inputs=t.inputs();atomic_json(t.record,{'version':t.version,'inputs':inputs,'source_sha256':fingerprint(inputs),'configuration_sha256':t.build_binding(),'dirty':False,'app_inventory':t.app_inventory(t.app()),'artifacts':{n:sha(t.out/n) for n in t.names}})
        return t.prepare(self.notes)
    def source_remotes(self):
        destinations = {}
        for service in ('github', 'gitlab'):
            path = self.root.parent/(self.root.name+'-'+service+'.git')
            subprocess.run(['git','init','--bare','-q',str(path)],check=True)
            self.addCleanup(__import__('shutil').rmtree,path)
            destinations[service] = str(path)
        return destinations

    def test_source_pushes_both_reviewed_commits(self):
        destinations = self.source_remotes()
        head = rel.commit(self.root, True)
        config = dict(self.config, source_branch='main')
        original = rel.git
        calls = []
        def recording(root, *args):
            if args[0] == 'push': calls.append(args)
            return original(root, *args)
        with patch.object(rel,'source_destination',side_effect=lambda config,service:destinations[service]), patch.object(rel,'git',side_effect=recording):
            rel.push_source(config,['github','gitlab'],dry_run=True,root=self.root)
            for destination in destinations.values():
                self.assertFalse(rel.git(self.root,'ls-remote','--heads',destination))
            rel.push_source(config,['github','gitlab'],confirmed=True,root=self.root)
        self.assertTrue(all(args[-1] == head+':refs/heads/main' for args in calls))
        for destination in destinations.values():
            self.assertEqual(rel.git(self.root,'ls-remote','--heads',destination).split()[0],head)

    def test_source_private_tracked_file_refused(self):
        (self.root/'.local').mkdir()
        (self.root/'.local/private.txt').write_text('private fixture')
        for args in [('add','.local/private.txt'),('commit','-qm','Private fixture')]:
            rel.git(self.root,*args)
        with patch.object(rel,'source_destination',return_value='unused'):
            with self.assertRaisesRegex(ReleaseError,'Private tracked'):
                rel.push_source(self.config,['github'],dry_run=True,root=self.root)

    def test_source_requires_confirmation_and_clean_checkout(self):
        with self.assertRaisesRegex(ReleaseError,'confirm-push'):
            rel.push_source(self.config,['github'],root=self.root)
        (self.root/'README.md').write_text('modified')
        with patch.object(rel,'source_destination',return_value='unused'):
            with self.assertRaisesRegex(ReleaseError,'clean committed'):
                rel.push_source(self.config,['github'],dry_run=True,root=self.root)

    def test_initialization_dry_run_never_changes_checkout(self):
        destinations = self.source_remotes()
        __import__('shutil').rmtree(self.root/'.git')
        before = {str(p.relative_to(self.root)):p.read_bytes() for p in self.root.rglob('*') if p.is_file()}
        env = dict(os.environ, GIT_AUTHOR_NAME='Fixture',GIT_AUTHOR_EMAIL='fixture@example.invalid',GIT_COMMITTER_NAME='Fixture',GIT_COMMITTER_EMAIL='fixture@example.invalid')
        with patch.dict(os.environ,env), patch.object(rel,'source_destination',side_effect=lambda config,service:destinations[service]):
            rel.push_source(self.config,['github','gitlab'],dry_run=True,initialize=True,root=self.root)
        self.assertFalse((self.root/'.git').exists())
        self.assertEqual(before,{str(p.relative_to(self.root)):p.read_bytes() for p in self.root.rglob('*') if p.is_file()})

    def test_imported_assets_in_ancestors_block_source_push(self):
        asset = self.root/'data/original.dat'
        asset.parent.mkdir();asset.write_bytes(b'original fixture')
        rel.git(self.root,'add','data/original.dat')
        rel.git(self.root,'commit','-qm','Imported fixture')
        rel.git(self.root,'rm','data/original.dat')
        rel.git(self.root,'commit','-qm','Removed fixture')
        with patch.object(rel,'source_destination',return_value='unused'):
            with self.assertRaisesRegex(ReleaseError,'content in source history'):
                rel.push_source(self.config,['github'],dry_run=True,root=self.root)
        with patch.object(self.tool,'tag_preflight') as transport:
            with self.assertRaisesRegex(ReleaseError,'content in source history'):
                self.tool.push_tag({'tag':'1.4.0'},'unused')
            transport.assert_not_called()

    def test_source_export_omits_content_and_history(self):
        from export_source import export
        from source_policy import excluded
        for name in ['data/original.dat','sound/music.ogg','res/icon.png','campgns/map.txt','deps/vendor.c','macos/packaging/controllers/controller-ps5.svg']:
            path=self.root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text('fixture')
        with tempfile.TemporaryDirectory() as temporary:
            destination=Path(temporary)/'source'
            export(self.root,destination)
            self.assertFalse((destination/'.git').exists())
            self.assertTrue((destination/'src/main.cpp').is_file())
            self.assertTrue((destination/'macos/packaging/controllers/controller-ps5.svg').is_file())
            self.assertFalse(any(excluded(str(p.relative_to(destination))) for p in destination.rglob('*') if p.is_file()))

    def test_signing_normalization_preserves_code_changes(self):
        header=struct.pack('<8I',0xfeedfacf,0,0,2,1,72,0,0)
        segment=struct.pack('<II16sQQQQiiII',0x19,72,b'__LINKEDIT',0,4096,104,8,0,0,0,0)
        original=header+segment+b'code1234';signed=bytearray(original);struct.pack_into('<Q',signed,64,8192)
        self.assertEqual(rel.normalized_macho(original),rel.normalized_macho(signed))
        signed[-1]^=1;self.assertNotEqual(rel.normalized_macho(original),rel.normalized_macho(signed))
        with self.assertRaises(ReleaseError):rel.normalized_macho(b'invalid')
    def test_manifest_and_staging(self):
        m=self.build();self.assertEqual(len(m['files']),4);self.assertEqual(m,self.tool.manifest_check());self.assertEqual(self.tool.prepared_check(m).name,'outputs')
    def test_artifact_change_rejected(self):
        self.build();(self.tool.out/self.tool.payload_names[0]).write_bytes(b'changed')
        with self.assertRaisesRegex(ReleaseError,'checksum'):self.tool.check()
    def test_source_change_rejected(self):
        self.build();(self.root/'README.md').write_text('changed')
        with self.assertRaisesRegex(ReleaseError,'Stale'):self.tool.check()
    def test_prepared_copy_change_rejected(self):
        m=self.build();(self.tool.prepared_check(m)/self.tool.payload_names[0]).write_bytes(b'changed')
        with self.assertRaisesRegex(ReleaseError,'Prepared asset'):self.tool.manifest_check()
    def test_notes_complete(self):
        self.build();self.notes.write_text('incomplete')
        with self.assertRaises(ReleaseError):self.tool.prepare(self.notes)
    def test_dirty_publication_rejected(self):
        self.build();(self.root/'README.md').write_text('dirty')
        with self.assertRaises(ReleaseError):self.tool.publish(['github'],True)
    def test_explicit_publication_and_destination(self):
        with self.assertRaisesRegex(ReleaseError,'confirm'):self.tool.publish(['github'],False)
        with self.assertRaises(ReleaseError):self.tool.publish(['github','github'],True)
    def test_private_input_and_symlink_refused(self):
        p=self.root/'private.key';p.write_text('private')
        with self.assertRaises(ReleaseError):self.tool.inputs()
        p.unlink();p.symlink_to(self.root/'README.md')
        with self.assertRaises(ReleaseError):self.tool.inputs()
    def test_output_path_refused(self):
        with self.assertRaises(ReleaseError):FixtureRelease(self.root,self.config,output='../outside')
        (self.root/'outputs').symlink_to('/tmp')
        with self.assertRaises(ReleaseError):FixtureRelease(self.root,self.config)
    def test_publish_preflights_all_before_tags(self):
        self.build();self.config.update(gitlab='https://gitlab.example.net/team/keeperfx-e',gitlab_git='git@gitlab.example.net:team/keeperfx-e.git');self.build()
        with patch.object(rel,'credential',return_value='fixture'),patch.object(self.tool,'tag_preflight',return_value=None),patch.object(self.tool,'push_tag') as push,patch.object(rel.GitHub,'preflight'),patch.object(rel.GitLab,'preflight',side_effect=ReleaseError('denied')):
            with self.assertRaises(ReleaseError):self.tool.publish(['github','gitlab'],True)
            push.assert_not_called()
    def test_receipt_preserves_completed_destinations(self):
        self.build()
        with patch.object(rel,'credential',return_value='fixture'),patch.object(self.tool,'tag_preflight',return_value=None),patch.object(self.tool,'push_tag'),patch.object(rel.GitHub,'preflight'),patch.object(rel.GitHub,'publish') as publish:
            receipt=self.tool.publish(['github'],True);self.assertEqual(receipt['completed'],['github']);self.tool.publish(['github'],True);self.assertEqual(publish.call_count,1)

    def test_github_draft_published_only_after_hashes(self):
        m = self.build();api = GithubAPI();adapter = rel.GitHub(m, self.tool.out, api)
        adapter.preflight();adapter.publish()
        self.assertFalse(api.r['draft'])
        self.assertIs(api.r['prerelease'],False)
        self.assertEqual(api.r['make_latest'],'true')
        self.assertEqual(api.r['name'],'Keeper-Enhanced 1.4.0')
        self.assertEqual(api.r['tag_name'],'1.4.0')
        patch_index = next(i for i, e in enumerate(api.events) if e[0] == 'PATCH')
        uploads = [i for i, e in enumerate(api.events) if 'uploads.github.com' in e[1]]
        self.assertEqual(len(uploads), len(m['files']))
        self.assertTrue(all(i < patch_index for i in uploads))
        adapter.publish()
        self.assertEqual(len([e for e in api.events if 'uploads.github.com' in e[1]]), len(m['files']))

    def test_github_existing_prerelease_promoted_after_verification(self):
        m=self.build();api=GithubAPI();adapter=rel.GitHub(m,self.tool.out,api)
        adapter.publish()
        api.r.update(prerelease=True,make_latest='false')
        adapter.publish()
        self.assertFalse(api.r['prerelease'])
        self.assertEqual(api.r['make_latest'],'true')

    def test_gitlab_normal_release_has_current_date_and_latest_check(self):
        m=self.build();m['gitlab']='https://gitlab.example.net/team/beam68'
        api=GitlabAPI(m['gitlab']);adapter=rel.GitLab(m,self.tool.out,api)
        adapter.publish()
        self.assertIn('released_at',api.r)
        self.assertEqual(api.r['name'],'Keeper-Enhanced 1.4.0')
        self.assertEqual(api.r['tag_name'],'1.4.0')
        self.assertIn(('GET',adapter.base+'/releases/permalink/latest'),api.events)
        api.r['upcoming_release']=True
        adapter.publish()
        self.assertFalse(api.r['upcoming_release'])

    def test_unknown_digest_and_conflict_keep_draft(self):
        m = self.build();api = GithubAPI();api.bad_digest = True
        with self.assertRaises(ReleaseError): rel.GitHub(m, self.tool.out, api).publish()
        self.assertTrue(api.r['draft'])
        with self.assertRaises(ReleaseError): rel.GitHub(m, self.tool.out, api).preflight()
        self.assertFalse(any(e[0] == 'PATCH' for e in api.events))

    def test_matching_github_assets_reused_after_interruption(self):
        m = self.build();api = GithubAPI();api.fail_upload = list(m['files'])[1]
        with self.assertRaises(ReleaseError): rel.GitHub(m, self.tool.out, api).publish()
        self.assertEqual(len(api.assets), 1)
        api.fail_upload = None
        rel.GitHub(m, self.tool.out, api).publish()
        self.assertEqual(len(api.assets), len(m['files']))

    def test_gitlab_downloads_before_advertising_and_refuses_conflict(self):
        m = self.build();m['gitlab'] = 'https://gitlab.example.net/team/nested/beam68'
        api = GitlabAPI(m['gitlab']);adapter = rel.GitLab(m, self.tool.out, api)
        adapter.preflight();adapter.publish()
        post = next(i for i, e in enumerate(api.events) if e[0] == 'POST')
        for name in m['files']:
            self.assertTrue(any(i < post and e == ('VERIFY', adapter.package_url(name)) for i,e in enumerate(api.events)))
        api.files[adapter.package_url(next(iter(m['files'])))] = b'conflict'
        with self.assertRaises(ReleaseError): adapter.preflight()

    def test_gitlab_corruption_never_creates_release(self):
        m = self.build();m['gitlab'] = 'https://gitlab.example.net/team/beam68'
        api = GitlabAPI(m['gitlab']);api.corrupt = True
        with self.assertRaises(ReleaseError): rel.GitLab(m, self.tool.out, api).publish()
        self.assertIsNone(api.r)

    def test_download_redirect_does_not_forward_token_to_storage(self):
        import urllib.error
        class Opener:
            def __init__(self): self.requests=[]
            def open(self,request,timeout):
                self.requests.append(request)
                if len(self.requests)==1:
                    raise urllib.error.HTTPError(request.full_url,302,'redirect',{'Location':'https://storage.example/signed'},None)
                return io.BytesIO(b'bytes')
        api=rel.API('https://gitlab.example.net','not-a-real-token');opener=Opener();api.opener=opener
        self.assertEqual(api.download_hash('https://gitlab.example.net/package',5),hashlib.sha256(b'bytes').hexdigest())
        self.assertTrue(any(k.lower()=='private-token' for k in opener.requests[0].headers))
        self.assertFalse(any(k.lower()=='private-token' for k in opener.requests[1].headers))

    def test_password_reference_resolved_only_in_memory(self):
        config=copy.deepcopy(self.config)
        config['credentials']={'github':{'provider':'1password','reference':'op://Test/Repository/token'}}
        result=subprocess.CompletedProcess([],0,stdout='private-test-value',stderr='')
        with patch.object(rel.subprocess,'run',return_value=result) as command:
            self.assertEqual(rel.credential('github',config),'private-test-value')
            self.assertEqual(command.call_args.args[0],['op','read','--no-newline','op://Test/Repository/token'])
        self.assertNotIn('private-test-value',json.dumps(config))

    def test_named_password_field_ambiguity_stops_lookup(self):
        config=copy.deepcopy(self.config)
        config['credentials']={'gitlab':{'provider':'1password','vault':'Test','item':'Repo','field':'group/project-rw'}}
        data=[{'label':'group/project-rw','value':'one'},{'label':'group/project-rw','value':'two'}]
        with patch.object(rel.subprocess,'run',return_value=subprocess.CompletedProcess([],0,stdout=json.dumps(data),stderr='')):
            with self.assertRaisesRegex(ReleaseError,'ambiguous'): rel.credential('gitlab',config)

if __name__=="__main__":unittest.main()
