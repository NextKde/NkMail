#!/usr/bin/env python3
"""Publish an accepted CI artifact, without rebuilding or replacing a published release."""
import argparse
import base64
import configparser
import ctypes
import ctypes.util
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'packaging/flatpak'))
from bundle_metadata import bundle_metadata_and_size
REPO = 'NextKde/NkMail'
APP = 'org.nextkde.NkMail'


def run(*args):
    return subprocess.check_output(args, cwd=ROOT, text=True).strip()


def api(path):
    return json.loads(run('gh', 'api', f'repos/{REPO}/{path}'))


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def check_bundle_identity(path, expected):
    flatpak = ctypes.CDLL(ctypes.util.find_library('flatpak'))
    gio = ctypes.CDLL(ctypes.util.find_library('gio-2.0'))
    objects = ctypes.CDLL(ctypes.util.find_library('gobject-2.0'))
    gio.g_file_new_for_path.argtypes = [ctypes.c_char_p]
    gio.g_file_new_for_path.restype = ctypes.c_void_p
    flatpak.flatpak_bundle_ref_new.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    flatpak.flatpak_bundle_ref_new.restype = ctypes.c_void_p
    objects.g_object_unref.argtypes = [ctypes.c_void_p]
    file = gio.g_file_new_for_path(os.fsencode(path))
    bundle = flatpak.flatpak_bundle_ref_new(file, None)
    try:
        if not bundle:
            raise ValueError('Invalid Flatpak bundle')
        for name, wanted in {'flatpak_ref_get_name': APP,
                             'flatpak_ref_get_arch': 'x86_64',
                             'flatpak_ref_get_branch': 'stable',
                             'flatpak_ref_get_commit': expected['commit'],
                             'flatpak_bundle_ref_get_origin': None,
                             'flatpak_bundle_ref_get_runtime_repo_url': None}.items():
            function = getattr(flatpak, name)
            function.argtypes = [ctypes.c_void_p]
            function.restype = ctypes.c_char_p
            value = function(bundle)
            if (value.decode() if value else None) != wanted:
                raise ValueError(f'Unexpected bundle property: {name}')
    finally:
        if bundle:
            objects.g_object_unref(bundle)
        if file:
            objects.g_object_unref(file)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run-id', required=True, type=int)
    args = parser.parse_args()
    if args.run_id <= 0 or os.environ.get('GITHUB_REPOSITORY') != REPO:
        raise ValueError('Use the official repository and a positive run ID')
    build = api(f'actions/runs/{args.run_id}')
    revision = build['head_sha']
    if (build['path'] != '.github/workflows/flatpak.yml'
            or build['conclusion'] != 'success'
            or not re.fullmatch('[0-9a-f]{40}', revision)):
        raise ValueError('A successful Flatpak workflow is required')
    subprocess.run(['git', 'merge-base', '--is-ancestor', revision, 'HEAD'], cwd=ROOT, check=True)
    gates = api(f'actions/runs?head_sha={revision}&per_page=100')['workflow_runs']
    if not any(item['path'] == '.github/workflows/ci.yml' and item['conclusion'] == 'success' for item in gates):
        raise ValueError('The release-gate workflow must have succeeded on the artifact source')
    work = ROOT / 'tmp' / f'publish-flatpak-{args.run_id}'
    work.mkdir(parents=True, exist_ok=False)
    run('gh', 'run', 'download', str(args.run_id), '--repo', REPO,
        '--name', 'nkmail-flatpak-x86_64', '--dir', str(work))
    plan = json.loads((work / 'flatpak-release.json').read_text())
    version = plan['version']
    if (plan['schemaVersion'] != 2 or plan['appId'] != APP or plan['revision'] != revision
            or not re.fullmatch(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)', version)
            or set(plan['bundles']) != {'x86_64'}):
        raise ValueError('Unexpected release identity')
    entry = plan['bundles']['x86_64']
    asset = f'{APP}-{version}-x86_64.flatpak'
    if entry['asset'] != asset or entry['ref'] != f'app/{APP}/x86_64/stable':
        raise ValueError('Unexpected application asset/ref')
    bundle = work / asset
    if digest(bundle) != entry['sha256'] or bundle.stat().st_size != entry['size']:
        raise ValueError('Application bundle digest/size mismatch')
    metadata, size = bundle_metadata_and_size(bundle)
    if (metadata != base64.b64decode(entry['metadata'], validate=True)
            or hashlib.sha256(metadata).hexdigest() != entry['metadataSha256']
            or size != entry['installedSize']):
        raise ValueError('Application metadata/installed size mismatch')
    config = configparser.ConfigParser(interpolation=None)
    config.read_string(metadata.decode())
    if (config['Application']['name'] != APP
            or config['Application']['runtime'] != 'org.kde.Platform/x86_64/6.11'
            or entry['runtime'] != {'ref': 'runtime/org.kde.Platform/x86_64/6.11'}
            or entry['requiredExtensions'] != []
            or config['Session Bus Policy']['org.NextKde.NkMail'] != 'own'):
        raise ValueError('Unexpected runtime or single-instance ownership')
    check_bundle_identity(bundle, entry)
    files = {asset, 'flatpak-release.json', f'{APP}.Sources-{version}-x86_64.flatpak',
             f'nkmail-{version}.tar.gz', f'{APP}.build-manifest.json',
             f'{APP}.build-environment.json', 'inbox.png', 'compose.png', 'account.png'}
    if {path.name for path in work.iterdir()} != files or any(path.is_symlink() or not path.is_file() for path in work.iterdir()):
        raise ValueError('Unexpected or missing artifact files')
    (work / 'SHA256SUMS').write_text(''.join(f'{digest(work / name)}  {name}\n' for name in sorted(files)))
    files.add('SHA256SUMS')
    tag = f'v{version}'
    releases = json.loads(run('gh', 'api', '--paginate', '--slurp', f'repos/{REPO}/releases?per_page=100'))
    matches = [release for page in releases for release in page if release['tag_name'] == tag]
    if len(matches) > 1:
        raise ValueError('Ambiguous release tag')
    if matches:
        release = matches[0]
        if not release['draft'] or release['target_commitish'] != revision or release['prerelease']:
            raise ValueError('Never overwrite a published release or a different draft source')
    else:
        notes = work / 'release-notes.txt'
        notes.write_text(f'NkMail {version}, Linux x86_64 Flatpak. Shared KDE runtime 6.11.\n\n'
                         f'Validated source: {revision}. Build and lifecycle acceptance: '
                         f'https://github.com/{REPO}/actions/runs/{args.run_id}\n\n'
                         'Includes the schema-2 release plan and matching sources. Synthetic offline '
                         'previews, single-instance activation, reinstall and data-preserving uninstall passed. '
                         'Application-password login; Google OAuth is not configured. Catalog signing is separate.\n')
        run('gh', 'release', 'create', tag, '--repo', REPO, '--target', revision, '--draft',
            '--title', f'NkMail {version} — Flatpak', '--notes-file', str(notes))
        release = next(item for item in api('releases') if item['tag_name'] == tag)
    existing = {item['name']: item for item in release['assets']}
    if set(existing) - files:
        raise ValueError('Draft contains unexpected assets')
    for name in sorted(files):
        path = work / name
        if name in existing:
            item = existing[name]
            if item['state'] != 'uploaded' or item['size'] != path.stat().st_size or item.get('digest') != 'sha256:' + digest(path):
                raise ValueError(f'Draft asset differs or is incomplete: {name}')
        else:
            run('gh', 'release', 'upload', tag, str(path), '--repo', REPO)
        print(f'Verified release asset: {name}', flush=True)
    uploaded = api(f"releases/{release['id']}")
    if (not uploaded['draft'] or uploaded['target_commitish'] != revision
            or len(uploaded['assets']) != len(files)):
        raise ValueError('Draft changed while uploading')
    for item in uploaded['assets']:
        path = work / item['name']
        if item['state'] != 'uploaded' or item['size'] != path.stat().st_size or item.get('digest') != 'sha256:' + digest(path):
            raise ValueError('Uploaded release asset verification failed')
    run('gh', 'release', 'edit', tag, '--repo', REPO, '--draft=false', '--latest')
    print(f'Published https://github.com/{REPO}/releases/tag/{tag}')


if __name__ == '__main__':
    main()
