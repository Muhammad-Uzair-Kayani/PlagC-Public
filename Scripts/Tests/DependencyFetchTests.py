"""Exercise recovery with local Git remotes; no network or vendor downloads."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

SCRIPT = Path(__file__).resolve().parents[2] / 'FetchDependencies.sh'
ENV = dict(os.environ, GIT_ALLOW_PROTOCOL='file', GIT_AUTHOR_NAME='Test',
           GIT_AUTHOR_EMAIL='test@example.invalid', GIT_COMMITTER_NAME='Test',
           GIT_COMMITTER_EMAIL='test@example.invalid')

def run(cwd, *args, ok=True):
    result = subprocess.run(args, cwd=cwd, env=ENV, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if ok and result.returncode:
        raise AssertionError(result.stdout)
    return result

with tempfile.TemporaryDirectory(prefix='plagc dependency tests ') as directory:
    root = Path(directory)
    vendor = root / 'source'
    vendor.mkdir()
    run(vendor, 'git', 'init', '-q')
    (vendor / 'marker.h').write_text('pinned\n')
    run(vendor, 'git', 'add', '.')
    run(vendor, 'git', 'commit', '-qm', 'pinned')
    pin = run(vendor, 'git', 'rev-parse', 'HEAD').stdout.strip()
    (vendor / 'marker.h').write_text('newer\n')
    run(vendor, 'git', 'commit', '-qam', 'newer')
    repo = root / 'parent'
    repo.mkdir()
    run(repo, 'git', 'init', '-q')
    (repo / '.gitmodules').write_text('[submodule "Vendor/demo"]\n path = Vendor/demo\n url = '+vendor.as_uri()+'\n shallow = true\n')
    (repo / 'Build').mkdir()
    (repo / 'Build/DependencyFiles.txt').write_text('Vendor/demo/marker.h\n')
    shutil.copyfile(SCRIPT, repo / 'FetchDependencies.sh')
    run(repo, 'git', 'add', '.')
    run(repo, 'git', 'update-index', '--add', '--cacheinfo', '160000,'+pin+',Vendor/demo')
    run(repo, 'git', 'commit', '-qm', 'parent')
    run(repo, 'bash', 'FetchDependencies.sh')
    dep = repo / 'Vendor/demo'
    assert run(dep, 'git', 'rev-parse', 'HEAD').stdout.strip() == pin
    assert run(dep, 'git', 'rev-parse', '--is-shallow-repository').stdout.strip() == 'true'
    print('PASS fresh clone fetches exact older pin shallowly')
    run(repo, 'bash', 'FetchDependencies.sh')
    print('PASS healthy rerun')
    (dep / 'marker.h').write_text('local edit\n')
    assert run(repo, 'bash', 'FetchDependencies.sh', ok=False).returncode != 0
    assert (dep / 'marker.h').read_text() == 'local edit\n'
    print('PASS dirty healthy checkout preserved and rejected')
    # Simulate an interrupted clone: .git pointer survives, metadata does not.
    shutil.rmtree(repo / '.git/modules/Vendor/demo')
    run(repo, 'bash', 'FetchDependencies.sh')
    backups = list((repo / '.git').glob('plagc-recovery.*'))
    assert len(backups) == 1
    assert (backups[0] / 'worktree/marker.h').read_text() == 'local edit\n'
    assert run(dep, 'git', 'rev-parse', 'HEAD').stdout.strip() == pin
    print('PASS stale pointer recovered with all original files backed up')
    # Incomplete metadata and no worktree .git file.
    shutil.rmtree(dep)
    shutil.rmtree(repo / '.git/modules/Vendor/demo')
    metadata = repo / '.git/modules/Vendor/demo'
    metadata.mkdir()
    (metadata / 'partial').write_text('preserve me')
    run(repo, 'bash', 'FetchDependencies.sh')
    assert any((b / 'gitdir/partial').exists() for b in (repo / '.git').glob('plagc-recovery.*'))
    assert run(dep, 'git', 'rev-parse', 'HEAD').stdout.strip() == pin
    print('PASS incomplete metadata preserved before reclone')
