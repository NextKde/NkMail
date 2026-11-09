#!/usr/bin/env python3
"""Disposable VM only: install and exercise synthetic, offline Flatpak previews."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

artifacts = Path(sys.argv[1]).resolve()
plan = json.loads((artifacts / 'flatpak-release.json').read_text())
entry = plan['bundles']['x86_64']
app = plan['appId']
bundle = artifacts / entry['asset']
assert hashlib.file_digest(bundle.open('rb'), 'sha256').hexdigest() == entry['sha256']

def run(*args, **kwargs):
    return subprocess.run(args, check=True, timeout=90, **kwargs)

run('flatpak', '--user', 'install', '--noninteractive', '--bundle', str(bundle))
try:
    actual = subprocess.check_output(['flatpak', 'info', '--user', '--show-commit', app], text=True).strip()
    assert actual == entry['commit']
    for scenario in ('inbox', 'compose', 'account'):
        # No actual account, saved credential or external network is used.
        run('flatpak', 'run', '--unshare=network', '--env=QT_QPA_PLATFORM=xcb',
            '--env=LIBGL_ALWAYS_SOFTWARE=1', app, '--demo', '--language', 'zh_CN',
            '--review-scenario', scenario, '--screenshot', f'/var/data/{scenario}.png')
        capture = Path.home() / '.var/app' / app / 'data' / f'{scenario}.png'
        assert capture.stat().st_size > 1000
        (artifacts / f'{scenario}.png').write_bytes(capture.read_bytes())
    marker = Path.home() / '.var/app' / app / 'data/packaging-fixture.txt'
    marker.write_text('Synthetic packaging fixture; no mailbox data.\n')
    run('flatpak', '--user', 'install', '--noninteractive', '--reinstall', '--bundle', str(bundle))
    assert marker.is_file()
finally:
    run('flatpak', '--user', 'uninstall', '--noninteractive', app)
assert marker.is_file(), 'Uninstall must preserve private application data'
print('PASS: exact bundle installed, three offline previews, reinstall, data-preserving uninstall')
