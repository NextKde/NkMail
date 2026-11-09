#!/usr/bin/env python3
"""Disposable VM only: install and exercise synthetic, offline Flatpak previews."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time

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
    # Screenshot mode intentionally bypasses instance ownership. Exercise the
    # normal sandbox launch separately to catch D-Bus permission mistakes.
    command = ['flatpak', 'run', '--unshare=network', '--env=QT_QPA_PLATFORM=xcb',
               '--env=LIBGL_ALWAYS_SOFTWARE=1', app, '--demo']
    with subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE) as primary:
        try:
            time.sleep(2)
            if primary.poll() is not None:
                output, errors = primary.communicate()
                raise RuntimeError(f'Primary demo exited {primary.returncode}: {output.decode()}\n{errors.decode()}')
            activated = subprocess.run(command, check=True, capture_output=True, text=True, timeout=30)
            assert 'Activated existing NkMail window.' in activated.stdout
            assert primary.poll() is None, 'Activation must preserve the primary instance'
        finally:
            if primary.poll() is None:
                run('flatpak', 'kill', app)
            primary.communicate(timeout=15)
    marker = Path.home() / '.var/app' / app / 'data/packaging-fixture.txt'
    marker.write_text('Synthetic packaging fixture; no mailbox data.\n')
    run('flatpak', '--user', 'uninstall', '--noninteractive', app)
    assert marker.is_file()
    run('flatpak', '--user', 'install', '--noninteractive', '--bundle', str(bundle))
    assert marker.is_file()
finally:
    run('flatpak', '--user', 'uninstall', '--noninteractive', app)
assert marker.is_file(), 'Uninstall must preserve private application data'
print('PASS: exact bundle installed, three offline previews, single-instance activation, reinstall, data-preserving uninstall')
