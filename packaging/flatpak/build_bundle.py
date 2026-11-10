#!/usr/bin/env python3
"""Build a pinned NkMail Flatpak and an appCatalog schema-2 manifest. No publication."""
import argparse
import base64
import configparser
import hashlib
import json
from pathlib import Path
import re
import subprocess
from bundle_metadata import bundle_metadata_and_size

ROOT = Path(__file__).resolve().parents[2]
APP_ID = 'org.nextkde.NkMail'


def run(*args):
    return subprocess.check_output(args, cwd=ROOT, text=True).strip()


def make_plan(bundle, repo, revision, version, arch):
    ref = f'app/{APP_ID}/{arch}/stable'
    metadata, installed_size = bundle_metadata_and_size(bundle)
    expected = subprocess.check_output(['ostree', f'--repo={repo}', 'cat', ref, '/metadata'])
    if metadata != expected:
        raise ValueError('Bundle and repository metadata differ')
    config = configparser.ConfigParser(interpolation=None)
    config.read_string(metadata.decode())
    if config['Application']['name'] != APP_ID:
        raise ValueError('Unexpected bundle identity')
    if config.get('Session Bus Policy', 'org.NextKde.NkMail', fallback='') != 'own':
        raise ValueError('Single-instance bus ownership is required; talk must not override own')
    runtime = config['Application']['runtime']
    if runtime != f'org.kde.Platform/{arch}/6.11':
        raise ValueError('Unexpected runtime')
    for section in config.sections():
        if section.startswith('Extension ') and not config.getboolean(section, 'autodelete', fallback=False):
            raise ValueError('Required app extensions are not published by this producer')
    commit = run('ostree', f'--repo={repo}', 'rev-parse', ref)
    if not re.fullmatch('[0-9a-f]{64}', commit):
        raise ValueError('Invalid OSTree commit')
    return {'schemaVersion': 2, 'appId': APP_ID, 'version': version, 'revision': revision,
            'bundles': {arch: {'asset': bundle.name, 'ref': ref, 'commit': commit,
                'sha256': hashlib.file_digest(bundle.open('rb'), 'sha256').hexdigest(),
                'size': bundle.stat().st_size, 'installedSize': installed_size,
                'metadata': base64.b64encode(metadata).decode(),
                'metadataSha256': hashlib.sha256(metadata).hexdigest(),
                'runtime': {'ref': 'runtime/' + runtime}, 'requiredExtensions': []}}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/flatpak')
    parser.add_argument('--arch', choices=['x86_64'], default='x86_64')
    args = parser.parse_args()
    work = args.output.resolve()
    work.mkdir(parents=True, exist_ok=True)
    artifacts = work / 'artifacts'
    artifacts.mkdir(exist_ok=True)
    revision = run('git', 'rev-parse', 'HEAD')
    if run('git', 'status', '--porcelain', '--untracked-files=no'):
        raise ValueError('Build from a clean committed source checkout')
    subprocess.run(['python3', 'scripts/check_ui_dependency.py'], cwd=ROOT, check=True)
    version = re.search(r'project\(NkMail VERSION ([0-9.]+)', (ROOT / 'CMakeLists.txt').read_text()).group(1)
    manifest = json.loads((ROOT / f'packaging/flatpak/{APP_ID}.json').read_text())
    manifest['modules'][-1]['sources'] = [{'type': 'git', 'url': 'https://github.com/NextKde/NkMail.git', 'commit': revision}]
    manifest_file = work / f'{APP_ID}.json'
    manifest_file.write_text(json.dumps(manifest, indent=2) + '\n')
    environment = {}
    for role in ('runtime', 'sdk', 'base'):
        ref = f"{manifest[role]}/{args.arch}/6.11"
        environment[role] = {'ref': ref, 'commit': run('flatpak', 'info', '--user', '--show-commit', ref)}
    repo = work / 'repo'
    subprocess.run(['flatpak-builder', '--user', f'--arch={args.arch}', '--default-branch=stable',
                    '--disable-rofiles-fuse', '--disable-tests', '--bundle-sources', '--jobs=2',
                    f'--state-dir={work / "state"}', f'--repo={repo}', str(work / 'app'), str(manifest_file)], check=True)
    for entry in environment.values():
        if run('flatpak', 'info', '--user', '--show-commit', entry['ref']) != entry['commit']:
            raise ValueError('Build dependency changed during the build')
    bundle = artifacts / f'{APP_ID}-{version}-{args.arch}.flatpak'
    subprocess.run(['flatpak', 'build-bundle', f'--arch={args.arch}', str(repo), str(bundle), APP_ID, 'stable'], check=True)
    plan = make_plan(bundle, repo, revision, version, args.arch)
    (artifacts / 'flatpak-release.json').write_text(json.dumps(plan, indent=2) + '\n')
    # The matching source ref includes bundled dependency sources for license compliance.
    subprocess.run(['flatpak', 'build-bundle', '--runtime', f'--arch={args.arch}', str(repo),
                    str(artifacts / f'{APP_ID}.Sources-{version}-{args.arch}.flatpak'), APP_ID + '.Sources', 'stable'], check=True)
    (artifacts / f'{APP_ID}.build-manifest.json').write_text(manifest_file.read_text())
    (artifacts / f'{APP_ID}.build-environment.json').write_text(json.dumps(environment, indent=2) + '\n')
    subprocess.run(['python3', 'scripts/export_release_source.py', str(artifacts)], cwd=ROOT, check=True)
    print(f'Prepared {bundle.name} at source {revision}; runtime org.kde.Platform/{args.arch}/6.11')


if __name__ == '__main__':
    main()
