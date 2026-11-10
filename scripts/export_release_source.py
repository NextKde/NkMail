#!/usr/bin/env python3
"""Export committed source, including pinned UI, without local files or credentials."""
import argparse
import gzip
import io
import re
from pathlib import Path
import subprocess
import tarfile

parser = argparse.ArgumentParser()
parser.add_argument('output', type=Path)
parser.add_argument('--version')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
version = args.version or re.search(r'project\(NkMail VERSION ([0-9.]+)', (root / 'CMakeLists.txt').read_text()).group(1)
if not re.fullmatch(r'[0-9][0-9A-Za-z.-]*', version):
    parser.error('Invalid source archive version')
prefix = f'nkmail-{version}'
sha = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip()
epoch = int(subprocess.check_output(['git', 'show', '-s', '--format=%ct', 'HEAD'], cwd=root))
ui = 'third_party/nextkdeAppDemo'
pin = subprocess.check_output(['git', 'rev-parse', f'HEAD:{ui}'], cwd=root, text=True).strip()
args.output.mkdir(parents=True, exist_ok=True)
archive_path = args.output / f'{prefix}.tar.gz'
with archive_path.open('wb') as raw, gzip.GzipFile(filename='', fileobj=raw, mode='wb', mtime=epoch) as gz, tarfile.open(fileobj=gz, mode='w') as output:
    for repository, revision, destination in ((root, sha, prefix), (root / ui, pin, f'{prefix}/{ui}')):
        archive = subprocess.check_output(['git', 'archive', '--format=tar', f'--prefix={destination}/', revision], cwd=repository)
        with tarfile.open(fileobj=io.BytesIO(archive)) as source:
            for member in source:
                member.uid = member.gid = 0
                member.uname = member.gname = 'root'
                member.mtime = epoch
                output.addfile(member, source.extractfile(member) if member.isfile() else None)
    content = f'NkMail: {sha}\nKos.Ui: {pin}\n'.encode()
    member = tarfile.TarInfo(f'{prefix}/SOURCE_REVISION')
    member.size, member.mtime, member.mode = len(content), epoch, 0o644
    output.addfile(member, io.BytesIO(content))
print(archive_path)
