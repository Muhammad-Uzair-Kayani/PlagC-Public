@echo off
setlocal DisableDelayedExpansion
cd /d "%~dp0" || exit /b 1
set "GITPUSH_MODE="
if "%~1"=="--check" set "GITPUSH_MODE=--check"
if exist "%~dp0.tools\venv\Scripts\python.exe" (
    "%~dp0.tools\venv\Scripts\python.exe" -I -c "import sys; p=open(sys.argv[1],encoding='utf-8-sig').read(); exec(compile(p.split('\n# GITPUSH_PYTHON\n',1)[1],sys.argv[1],'exec'),{'__name__':'__main__','__file__':sys.argv[1]})" "%~f0" "%GITPUSH_MODE%"
    goto done
)
where py >nul 2>&1
if not errorlevel 1 (
    py -3 -I -c "import sys; p=open(sys.argv[1],encoding='utf-8-sig').read(); exec(compile(p.split('\n# GITPUSH_PYTHON\n',1)[1],sys.argv[1],'exec'),{'__name__':'__main__','__file__':sys.argv[1]})" "%~f0" "%GITPUSH_MODE%"
    goto done
)
where python >nul 2>&1
if not errorlevel 1 (
    python -I -c "import sys; p=open(sys.argv[1],encoding='utf-8-sig').read(); exec(compile(p.split('\n# GITPUSH_PYTHON\n',1)[1],sys.argv[1],'exec'),{'__name__':'__main__','__file__':sys.argv[1]})" "%~f0" "%GITPUSH_MODE%"
    goto done
)
echo Python is required. Install Python 3.10+ or run the project Setup.bat.
exit /b 1
:done
set "GITPUSH_RESULT=%errorlevel%"
echo.
if "%GITPUSH_MODE%"=="--check" exit /b %GITPUSH_RESULT%
pause
exit /b %GITPUSH_RESULT%
# GITPUSH_PYTHON
"""Interactive GitHub upload, with explicit destinations and no history rewriting."""
import datetime
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import uuid
import json

class Stop(RuntimeError):
    pass

class Push:
    def __init__(self, root, ask=input, allow_local=False):
        self.root=Path(root).resolve(); self.ask=ask; self.allow_local=allow_local
        self.env=os.environ.copy(); self.env['GIT_TERMINAL_PROMPT']='1'
        self.env['GIT_LITERAL_PATHSPECS']='0'
        self.backup=None
    def git(self,*args,check=True,data=None,cwd=None):
        result=subprocess.run(['git','-c','core.longpaths=true',*args],cwd=cwd or self.root,
            env=self.env,input=data,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        if check and result.returncode:
            raise Stop(result.stderr.decode('utf-8','replace').strip() or
                       'Git failed: '+args[0])
        return result
    def text(self,*args,**kw):
        return self.git(*args,**kw).stdout.decode('utf-8','replace').strip()
    def url(self,value):
        if self.allow_local and Path(value).is_absolute(): return value
        if not re.fullmatch(r'(https://github\.com/|git@github\.com:)[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+/?',value):
            raise Stop('Use https://github.com/OWNER/REPO.git or git@github.com:OWNER/REPO.git. '
                       'Use Git Credential Manager/SSH for authentication; do not put tokens in the URL.')
        return value.rstrip('/')
    def branch(self,value):
        if not value or value.startswith('-') or self.git('check-ref-format','--branch',value,check=False).returncode:
            raise Stop('Invalid branch name.')
        return value
    def preflight(self):
        if not shutil.which('git'): raise Stop('Install Git for Windows and reopen the terminal.')
        top=self.git('rev-parse','--show-toplevel',check=False)
        if top.returncode: raise Stop('This folder is not a Git checkout. Clone your destination repository first, then copy the project into it.')
        if Path(os.fsdecode(top.stdout).strip()).resolve()!=self.root:
            raise Stop('Place GitPush.bat at the actual repository root. Refusing to stage a parent repository.')
        for state in ('MERGE_HEAD','CHERRY_PICK_HEAD','REVERT_HEAD','rebase-merge','rebase-apply','BISECT_LOG','sequencer','index.lock'):
            path=Path(self.text('rev-parse','--git-path',state))
            if not path.is_absolute():path=self.root/path
            if path.exists():raise Stop('Finish/review the existing Git operation first: '+state+'. No lock or operation was removed.')
        if self.git('ls-files','-u','-z').stdout: raise Stop('Resolve existing conflicted files before uploading.')
        current=self.text('symbolic-ref','--quiet','--short','HEAD',check=False)
        if not current:raise Stop('Detached HEAD: switch to a local branch before uploading.')
        return current
    def identity(self):
        for key,prompt in [('user.name','Commit author name'),('user.email','Commit author email')]:
            if not self.text('config','--get',key,check=False):
                value=self.ask(prompt+': ').strip()
                if not value or any(c in value for c in '\r\n\0'):raise Stop('An author name/email is required.')
                self.git('config','--local',key,value)
        self.git('var','GIT_AUTHOR_IDENT');self.git('var','GIT_COMMITTER_IDENT')
    def destination(self,current):
        existing=self.text('remote','get-url','origin',check=False)
        try:display=self.url(existing) if existing else '(not configured)'
        except Stop:display='(unsupported or credential-bearing URL; enter a clean GitHub URL)'
        print('Current origin:',display)
        requested=self.ask('GitHub repository URL [Enter keeps current]: ').strip() or existing
        requested=self.url(requested)
        if existing:
            if requested!=existing:self.git('remote','set-url','origin',requested)
        else:self.git('remote','add','origin',requested)
        push_urls=self.text('remote','get-url','--push','--all','origin').splitlines()
        if push_urls!=[requested]:raise Stop('origin has a separate/multiple push URL. Review git remote -v and remove or correct that configuration first.')
        if self.text('config','--bool','--get','remote.origin.mirror',check=False)=='true':
            raise Stop('Mirror remotes are not supported. Configure a normal origin remote.')
        default=self.text('config','--get','lastlight.pushBranch',check=False) or current
        target=self.branch(self.ask('Destination branch ['+default+']: ').strip() or default)
        while True:
            print('Checking access and fetching the destination branch...')
            tips=self.text('ls-remote','--heads','origin','refs/heads/'+target)
            tip=tips.split()[0] if tips else None
            if tip:
                self.git('fetch','--no-tags','origin','refs/heads/'+target)
                fetched=self.text('rev-parse','FETCH_HEAD')
                if fetched!=tip:raise Stop('Remote branch changed during fetch. Run GitPush again.')
                head=self.text('rev-parse','--verify','HEAD',check=False)
                related=self.git('merge-base','--is-ancestor',tip,head,check=False) if head else None
                if related and related.returncode not in (0,1):raise Stop('Could not verify remote ancestry.')
                if not related or related.returncode:
                    print('The destination has commits absent from this local branch.')
                    print('Automatic merge/rebase and force-push are disabled. Existing remote history will be preserved.')
                    target=self.ask('Enter a NEW remote branch name to upload separately, or Enter to cancel: ').strip()
                    if not target:raise Stop('Cancelled. Integrate the remote history manually, or retry with a new branch.')
                    target=self.branch(target)
                    if self.text('ls-remote','--heads','origin','refs/heads/'+target):
                        raise Stop('That branch already exists. Choose an unused name on the next run.')
                    continue
            return target,tip
    def save_index(self):
        index=Path(self.text('rev-parse','--git-path','index'))
        if not index.is_absolute():index=self.root/index
        directory=index.parent/'lastlight-push-backups'/(datetime.datetime.now().strftime('%Y%m%d-%H%M%S')+'-'+uuid.uuid4().hex[:8])
        directory.mkdir(parents=True)
        if index.exists():shutil.copy2(index,directory/'index.before')
        (directory/'staged-before.patch').write_bytes(self.git('diff','--cached','--binary').stdout)
        if (self.root/'.gitmodules').exists():shutil.copy2(self.root/'.gitmodules',directory/'gitmodules.before')
        self.backup=directory
        print('Original staging backup:',directory)
    def engine(self):
        lock=self.root/'godot.lock.json'
        if not lock.exists(): return None
        try:spec=json.loads(lock.read_text(encoding='utf-8-sig'));pin=spec['commit'];url=self.url(spec['repository']);tag=spec['tag']
        except (ValueError,KeyError) as e:raise Stop('Invalid godot.lock.json: '+str(e))
        if not re.fullmatch('[0-9a-fA-F]{40}',pin):raise Stop('Godot lock must contain a full 40-character commit.')
        if not tag or self.git('check-ref-format','refs/tags/'+tag,check=False).returncode:raise Stop('Invalid Godot release tag.')
        # Verify the pinned release is actually available from the advertised upstream.
        refs=self.text('ls-remote',url,'refs/tags/'+tag,'refs/tags/'+tag+'^{}')
        commits={line.split()[0] for line in refs.splitlines()}
        if pin not in commits:raise Stop('The locked Godot commit does not match the upstream release tag. Review the lock; no engine files were changed.')
        path='vendor/godot';engine=self.root/path
        history=self.git('ls-tree','-r','HEAD','--',path,check=False).stdout
        if any(not line.startswith(b'160000 ') for line in history.splitlines()):
            raise Stop('Godot loose files already exist in local commit history. Review/clean that history manually before publishing; this tool will not rewrite it.')
        top=self.git('rev-parse','--show-toplevel',check=False,cwd=engine) if engine.is_dir() else None
        if top and top.returncode==0 and Path(os.fsdecode(top.stdout).strip()).resolve()==engine.resolve():
            if self.git('status','--porcelain','-z',cwd=engine).stdout:
                raise Stop('Godot has local edits or missing tracked files. Preserve/review them before pushing; engine changes need their own repository/commit.')
            if self.text('rev-parse','HEAD',cwd=engine)!=pin:raise Stop('Local Godot HEAD differs from godot.lock.json. Review it before uploading.')
        elif engine.exists():
            print('Godot is a loose/uninitialized copied folder. Files will stay untouched; only the verified upstream release will be published as a submodule.')
            if self.ask('Exclude the copied engine files and record the locked upstream release? [y/N]: ').strip().lower()!='y':
                raise Stop('Cancelled. Review any local engine edits before recording the upstream pin.')
        modules=self.root/'.gitmodules'
        if modules.is_symlink():raise Stop('.gitmodules is a symbolic link; review it manually first.')
        if modules.exists():
            matches=self.text('config','-f',str(modules),'--get-regexp',r'^submodule\..*\.path$',check=False)
            sections=[line.rsplit(' ',1)[0][:-5] for line in matches.splitlines() if line.rsplit(' ',1)[-1]==path]
            if len(sections)>1:raise Stop('Duplicate Godot entries in .gitmodules; review them first.')
            section=sections[0] if sections else 'submodule.vendor/godot'
        else:section='submodule.vendor/godot'
        return path,pin,url,section
    def stage(self,engine):
        self.save_index()
        if engine:
            path,pin,url,section=engine
            entries=self.git('ls-files','--stage','-z','--',path).stdout.split(b'\0')
            loose=[]
            for entry in entries:
                if entry:
                    mode,name=entry.split(b' ',1)[0],entry.split(b'\t',1)[1]
                    if mode!=b'160000':loose.append(name)
            if loose:self.git('update-index','--force-remove','-z','--stdin',data=b'\0'.join(loose)+b'\0')
            self.git('config','-f','.gitmodules',section+'.path',path)
            self.git('config','-f','.gitmodules',section+'.url',url)
            self.git('update-index','--add','--cacheinfo','160000',pin,path)
            # Never descend into a loose copied engine, even after a previous partial git add.
            self.git('add','-A','--','.',':(exclude)'+path)
            self.git('add','--','.gitmodules')
            if not self.git('ls-files','--stage','--',path).stdout.startswith(('160000 '+pin+' ').encode()):
                raise Stop('Engine gitlink verification failed.')
        else:self.git('add','-A','--','.')
        entries=self.git('ls-files','--stage','-z').stdout.split(b'\0');oids=[]
        for entry in entries:
            if not entry:continue
            meta,name=entry.split(b'\t',1);mode,oid,stage=meta.split()
            if stage!=b'0':raise Stop('Staging contains unresolved conflicts.')
            if mode==b'160000':
                if engine and name==engine[0].encode():continue
                raise Stop('Additional submodules require manual publication verification: '+os.fsdecode(name))
            parts=os.fsdecode(name).replace('\\','/').split('/')
            if parts[0] in ('.tools','build','dist','reports','.vs') or parts[:2]==['game','.godot']:
                raise Stop('Generated files are already tracked: '+os.fsdecode(name)+'. Remove generated paths from the index with git rm --cached, then retry.')
            if Path(os.fsdecode(name)).name.lower() in ('.env','id_rsa','id_ed25519','credentials.json'):
                raise Stop('Review this potentially private staged file before publishing: '+os.fsdecode(name))
            oids.append(oid.decode())
        self.large_objects(oids)
        if (self.root/'godot.lock.json').exists():
            if not (self.root/'game/game-content.pck').is_file():raise Stop('Required runtime assets missing: game/game-content.pck.')
            if not (self.root/'Deliverables/LastLight-Windows-x64.zip').is_file():
                print('NOTE: No playable Windows ZIP is included. This upload will require a source build to play.')
    def large_objects(self,oids):
        if not oids:return
        result=self.git('cat-file','--batch-check=%(objectname) %(objecttype) %(objectsize)',data=('\n'.join(sorted(set(oids)))+'\n').encode())
        for line in result.stdout.decode().splitlines():
            parts=line.split()
            if len(parts)!=3:raise Stop('Could not inspect a Git object.')
            if parts[1]=='blob' and int(parts[2])>=100*1024*1024:
                raise Stop('A Git blob reaches GitHub\'s 100 MiB limit: '+parts[0]+'. Use Git LFS or a Release asset and review existing history.')
    def run(self):
        current=self.preflight();self.identity();target,tip=self.destination(current)
        engine=self.engine();self.stage(engine)
        print('\nStaged changes:')
        print(self.text('diff','--cached','--stat'))
        print(self.text('status','--short','--ignore-submodules=all'))
        print('\nDestination: origin / '+target+' (local branch stays '+current+')')
        changed=self.git('diff','--cached','--quiet',check=False).returncode
        if changed not in (0,1):raise Stop('Cannot inspect staged changes.')
        if changed:
            message=self.ask('Commit message (Enter cancels): ').strip()
            if not message:raise Stop('Cancelled before commit.')
            if '\0' in message:raise Stop('Commit message contains a NUL character.')
            file=self.backup/'commit-message.txt';file.write_text(message+'\n',encoding='utf-8')
            self.git('commit','--file',str(file))
        else:
            if not self.text('rev-parse','--verify','HEAD',check=False):raise Stop('Nothing to commit or push.')
            if self.ask('No staged changes. Push existing local commits? [y/N]: ').strip().lower()!='y':raise Stop('Cancelled.')
        revision=self.text('rev-parse','HEAD')
        # Inspect unpublished history too: removing a huge file in a later commit is insufficient.
        args=['rev-list','--objects','--no-object-names',revision]
        if tip:args+=['^'+tip]
        self.large_objects(self.text(*args).splitlines())
        # Ordinary push rejects races/non-fast-forwards; never use --force or pull.
        result=self.git('-c','push.followTags=false','push','--porcelain','origin','HEAD:refs/heads/'+target,check=False)
        print(result.stdout.decode('utf-8','replace'));print(result.stderr.decode('utf-8','replace'))
        if result.returncode:raise Stop('Push failed. Your local commit '+revision+' is preserved. Review the server/authentication/hook error above and rerun. No force-push was attempted.')
        published=self.text('ls-remote','--heads','origin','refs/heads/'+target).split()
        if not published or published[0]!=revision:raise Stop('Push returned success but remote verification changed/failed. Check GitHub before retrying.')
        self.git('config','--local','lastlight.pushBranch',target)
        print('VERIFIED: '+target+' now points to '+revision)

def main(root=None):
    for stream in (sys.stdout,sys.stderr):
        if hasattr(stream,'reconfigure'):stream.reconfigure(errors='replace')
    if root is None:
        root=Path(__file__).resolve().parent
        if Path(__file__).suffix=='.py':root=root.parent
    job=Push(root)
    try:
        if '--check' in sys.argv:
            branch=job.preflight();print('GitPush launcher ready. Repository:',job.root,'Branch:',branch);return 0
        job.run();return 0
    except (Stop,OSError,KeyboardInterrupt,EOFError) as e:
        print('\nSTOPPED:',str(e) or 'Cancelled.')
        print('Working files were not deleted or reset. Staging/configuration may have changed.')
        if job.backup:print('Original index/.gitmodules backup:',job.backup)
        return 1
if __name__=='__main__':sys.exit(main())
