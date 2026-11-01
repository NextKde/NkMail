#!/usr/bin/env python3
"""Read-only pin gate for the canonical Kos.Ui submodule."""

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
from urllib.error import HTTPError, URLError
from urllib.parse import quote
from urllib.request import Request, urlopen


UI_PATH = "third_party/nextkdeAppDemo"
UI_URL = "https://github.com/NextKde/nextkdeAppDemo.git"
LOCK_PATH = "third_party/kos-ui.lock.json"
API = "https://api.github.com/repos/NextKde/nextkdeAppDemo"
SHA = re.compile(r"[0-9a-f]{40}\Z")
TAG = re.compile(r"v\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?\Z")


class GateError(Exception):
    pass


def git(root, *args):
    result = subprocess.run(
        ["git", "-C", str(root), *args], capture_output=True, text=True, timeout=30
    )
    if result.returncode:
        raise GateError(f"Git check failed ({' '.join(args[:2])}); initialize the submodule and check the repository state.")
    return result.stdout.strip()


def local_dependency(root):
    root = Path(root).resolve()
    if Path(git(root, "rev-parse", "--show-toplevel")).resolve() != root:
        raise GateError("Run this check against the NkMail repository root.")
    try:
        lock = json.loads((root / LOCK_PATH).read_text())
    except (OSError, ValueError) as exc:
        raise GateError(f"Cannot read {LOCK_PATH}.") from exc
    if not isinstance(lock, dict) or "sha" not in lock or not set(lock) <= {"sha", "tag"}:
        raise GateError(
            'UI lock must be {"sha": "<40-hex>"} or {"sha": "<40-hex>", "tag": "vX.Y.Z"}; '
            "no other keys or a missing sha are allowed."
        )
    tag = lock.get("tag")
    if "tag" in lock and (not isinstance(tag, str) or not TAG.fullmatch(tag)):
        raise GateError("UI tag must be a concrete version such as v0.1.0-rc.1, not main/latest.")
    if not isinstance(lock["sha"], str) or not SHA.fullmatch(lock["sha"]):
        raise GateError("UI lock sha must be a full lowercase commit SHA.")
    sha = lock["sha"]

    # Compare the staged snapshot, so contributors can check a deliberate upgrade
    # after staging its gitlink and lock, without committing it first.
    for path in (".gitmodules", LOCK_PATH):
        if git(root, "show", f":{path}") != (root / path).read_text().strip():
            raise GateError(f"Stage the intended {path} change before checking the UI pin.")
    config = git(root, "config", "--file", ".gitmodules", "--list").splitlines()
    expected = {
        f"submodule.{UI_PATH}.path={UI_PATH}",
        f"submodule.{UI_PATH}.url={UI_URL}",
    }
    if len(config) != 2 or set(config) != expected:
        raise GateError(".gitmodules must use the canonical UI path/HTTPS URL without a tracking branch or overrides.")
    entries = git(root, "ls-files", "--stage", "--", UI_PATH).splitlines()
    if len(entries) != 1 or entries[0] != f"160000 {sha} 0\t{UI_PATH}":
        raise GateError("Staged UI gitlink and lock SHA differ; stage both for the chosen pin.")
    module = root / UI_PATH
    if not (module / ".git").exists():
        raise GateError("UI submodule is not initialized. Run git submodule update --init --recursive.")
    if Path(git(module, "rev-parse", "--show-toplevel")).resolve() != module.resolve():
        raise GateError("UI path is not an initialized independent Git submodule.")
    if git(module, "rev-parse", "HEAD") != sha:
        raise GateError("UI HEAD differs from the staged pin; check out the chosen commit and stage the gitlink.")
    flags = git(module, "ls-files", "-v").splitlines()
    if any(line and (line[0].islower() or line[0] == "S") for line in flags):
        raise GateError("UI index hides files with assume-unchanged/skip-worktree; clear those flags before verification.")
    if git(module, "status", "--porcelain", "--untracked-files=all", "--ignore-submodules=none"):
        raise GateError("UI submodule has uncommitted/untracked changes. Commit and push shared changes upstream first.")
    return tag, sha


def api_get(path):
    headers = {"Accept": "application/vnd.github+json", "User-Agent": "NkMail-UI-dependency-gate", "X-GitHub-Api-Version": "2022-11-28"}
    token = os.environ.get("GITHUB_TOKEN")
    if token:
        headers["Authorization"] = f"Bearer {token}"
    try:
        with urlopen(Request(API + path, headers=headers), timeout=30) as response:
            data = json.load(response)
    except HTTPError as exc:
        raise GateError(f"GitHub upstream verification failed (HTTP {exc.code}); confirm the pinned UI commit exists upstream or resolve API access/rate limits.") from exc
    except (URLError, TimeoutError, OSError, ValueError) as exc:
        raise GateError("GitHub upstream verification unavailable; network/TLS/JSON failures cannot be skipped.") from exc
    if not isinstance(data, dict):
        raise GateError("Unexpected GitHub API response.")
    return data


def published_dependency(tag, sha, get=api_get):
    if tag is not None:
        ref = get("/git/ref/tags/" + quote(tag, safe=""))
        obj = ref.get("object") if isinstance(ref, dict) else None
        seen = set()
        for _ in range(8):
            if not isinstance(obj, dict) or not isinstance(obj.get("sha"), str) or not SHA.fullmatch(obj["sha"]):
                raise GateError("Invalid upstream tag object.")
            if obj.get("type") == "commit":
                if obj["sha"] != sha:
                    raise GateError("Upstream UI tag points to a different commit than the gitlink/lock.")
                break
            if obj.get("type") != "tag" or obj["sha"] in seen:
                raise GateError("Upstream UI tag does not resolve to a commit.")
            seen.add(obj["sha"])
            tag_ref = get("/git/tags/" + obj["sha"])
            obj = tag_ref.get("object") if isinstance(tag_ref, dict) else None
        else:
            raise GateError("Upstream UI tag nesting exceeds the verification limit.")
    commit = get("/commits/" + sha)
    if not isinstance(commit, dict) or commit.get("sha") != sha:
        raise GateError("Upstream UI commit does not exist for the pinned SHA.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    try:
        tag, sha = local_dependency(args.root)
        published_dependency(tag, sha)
    except (GateError, OSError, subprocess.TimeoutExpired) as exc:
        print(f"UI gate: FAIL: {exc}", file=sys.stderr)
        return 1
    chosen = f"tag {tag}, " if tag is not None else ""
    print(f"UI gate: PASS: {chosen}sha {sha} (clean canonical submodule, upstream commit exists).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
