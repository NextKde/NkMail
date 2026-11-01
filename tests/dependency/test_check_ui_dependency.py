import importlib.util
import io
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch
from urllib.error import HTTPError, URLError


ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("ui_gate", ROOT / "scripts/check_ui_dependency.py")
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)


def git(root, *args):
    return subprocess.check_output(["git", "-C", str(root), *args], text=True).strip()


def init(root):
    root.mkdir(parents=True)
    git(root, "init", "-q")
    git(root, "config", "user.name", "Fixture")
    git(root, "config", "user.email", "fixture@example.invalid")


class LocalGateTests(unittest.TestCase):
    def setUp(self):
        (ROOT / "tmp").mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=ROOT / "tmp", prefix="ui-gate-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "app"
        init(self.root)
        self.module = self.root / gate.UI_PATH
        init(self.module)
        (self.module / "control.qml").write_text("initial\n")
        git(self.module, "add", ".")
        git(self.module, "commit", "-qm", "fixture component")
        self.sha = git(self.module, "rev-parse", "HEAD")
        self.lock = self.root / gate.LOCK_PATH
        self.lock.write_text(json.dumps({"tag": "v0.1.0-rc.1", "sha": self.sha}))
        (self.root / ".gitmodules").write_text(
            f'[submodule "{gate.UI_PATH}"]\n\tpath = {gate.UI_PATH}\n\turl = {gate.UI_URL}\n'
        )
        git(self.root, "add", ".gitmodules", gate.LOCK_PATH)
        git(self.root, "update-index", "--add", "--cacheinfo", f"160000,{self.sha},{gate.UI_PATH}")
        git(self.root, "commit", "-qm", "pin fixture")

    def rejects(self, fragment):
        with self.assertRaisesRegex(gate.GateError, fragment):
            gate.local_dependency(self.root)

    def test_clean_pin(self):
        self.assertEqual(gate.local_dependency(self.root), ("v0.1.0-rc.1", self.sha))

    def test_uninitialized_even_inside_parent_repository(self):
        (self.module / ".git").rename(self.module / "git-hidden")
        self.rejects("not initialized")

    def test_dirty_tracked(self):
        (self.module / "control.qml").write_text("new\n")
        self.rejects("uncommitted/untracked")

    def test_dirty_index(self):
        (self.module / "control.qml").write_text("new\n")
        git(self.module, "add", "control.qml")
        self.rejects("uncommitted/untracked")

    def test_untracked_component(self):
        (self.module / "forgotten.qml").write_text("new\n")
        self.rejects("uncommitted/untracked")

    def test_local_ignore_setting_does_not_hide_changes(self):
        git(self.module, "config", "status.showUntrackedFiles", "no")
        git(self.root, "config", f"submodule.{gate.UI_PATH}.ignore", "all")
        (self.module / "forgotten.qml").write_text("new\n")
        self.rejects("uncommitted/untracked")

    def test_assume_unchanged_cannot_hide_changes(self):
        git(self.module, "update-index", "--assume-unchanged", "control.qml")
        (self.module / "control.qml").write_text("hidden change\n")
        self.rejects("index hides files")

    def test_skip_worktree_cannot_hide_changes(self):
        git(self.module, "update-index", "--skip-worktree", "control.qml")
        (self.module / "control.qml").write_text("hidden change\n")
        self.rejects("index hides files")

    def test_head_differs_from_pin(self):
        git(self.module, "commit", "--allow-empty", "-qm", "unpublished")
        self.rejects("HEAD differs")

    def test_lock_differs_from_gitlink(self):
        self.lock.write_text(json.dumps({"tag": "v0.1.0-rc.1", "sha": "a" * 40}))
        git(self.root, "add", gate.LOCK_PATH)
        self.rejects("gitlink and lock SHA differ")

    def test_unstaged_lock(self):
        self.lock.write_text(json.dumps({"tag": "v0.1.0-rc.2", "sha": self.sha}))
        self.rejects("Stage the intended")

    def test_wrong_url(self):
        git(self.root, "config", "--file", ".gitmodules", f"submodule.{gate.UI_PATH}.url", "https://example.invalid/ui.git")
        git(self.root, "add", ".gitmodules")
        self.rejects("canonical UI")

    def test_tracking_branch(self):
        git(self.root, "config", "--file", ".gitmodules", f"submodule.{gate.UI_PATH}.branch", "main")
        git(self.root, "add", ".gitmodules")
        self.rejects("tracking branch")

    def test_not_a_gitlink(self):
        git(self.root, "rm", "--cached", gate.UI_PATH)
        self.rejects("gitlink and lock SHA differ")

    def test_concrete_version_required(self):
        self.lock.write_text(json.dumps({"tag": "latest", "sha": self.sha}))
        self.rejects("concrete version")

    def test_malformed_lock(self):
        self.lock.write_text("[]")
        self.rejects("no other keys")

    def test_sha_only_lock(self):
        self.lock.write_text(json.dumps({"sha": self.sha}))
        git(self.root, "add", gate.LOCK_PATH)
        self.assertEqual(gate.local_dependency(self.root), (None, self.sha))

    def test_extra_lock_keys_rejected(self):
        self.lock.write_text(json.dumps({"sha": self.sha, "release": "v0.1.0-rc.1"}))
        git(self.root, "add", gate.LOCK_PATH)
        self.rejects("no other keys")

    def test_staged_upgrade_can_be_checked_before_parent_commit(self):
        git(self.module, "commit", "--allow-empty", "-qm", "next release")
        new_sha = git(self.module, "rev-parse", "HEAD")
        self.lock.write_text(json.dumps({"tag": "v0.1.0-rc.2", "sha": new_sha}))
        git(self.root, "add", gate.LOCK_PATH)
        git(self.root, "update-index", "--cacheinfo", f"160000,{new_sha},{gate.UI_PATH}")
        self.assertEqual(gate.local_dependency(self.root), ("v0.1.0-rc.2", new_sha))


class UpstreamGateTests(unittest.TestCase):
    def setUp(self):
        self.sha = "a" * 40
        self.tag_sha = "b" * 40
        self.responses = {
            "/git/ref/tags/v0.1.0-rc.1": {"object": {"type": "tag", "sha": self.tag_sha}},
            "/git/tags/" + self.tag_sha: {"object": {"type": "commit", "sha": self.sha}},
            "/commits/" + self.sha: {"sha": self.sha},
        }

    def check(self, tag="v0.1.0-rc.1"):
        gate.published_dependency(tag, self.sha, self.responses.__getitem__)

    def test_annotated_tag_and_existing_commit(self):
        self.check()

    def test_lightweight_tag(self):
        self.responses["/git/ref/tags/v0.1.0-rc.1"]["object"] = {"type": "commit", "sha": self.sha}
        self.check()

    def test_sha_only_pin_skips_tag_resolution(self):
        gate.published_dependency(None, self.sha, lambda key: {"/commits/" + self.sha: {"sha": self.sha}}[key])

    def test_wrong_tag_commit(self):
        self.responses["/git/tags/" + self.tag_sha]["object"]["sha"] = "c" * 40
        with self.assertRaisesRegex(gate.GateError, "different commit"):
            self.check()

    def test_tag_cycle(self):
        self.responses["/git/tags/" + self.tag_sha]["object"] = {"type": "tag", "sha": self.tag_sha}
        with self.assertRaisesRegex(gate.GateError, "resolve to a commit"):
            self.check()

    def test_invalid_tag_object(self):
        self.responses["/git/ref/tags/v0.1.0-rc.1"]["object"] = {"type": "commit", "sha": "main"}
        with self.assertRaisesRegex(gate.GateError, "Invalid upstream tag"):
            self.check()

    def test_missing_upstream_commit(self):
        def get(key):
            if key == "/commits/" + self.sha:
                raise gate.GateError("HTTP 404")
            return self.responses[key]
        with self.assertRaisesRegex(gate.GateError, "404"):
            gate.published_dependency("v0.1.0-rc.1", self.sha, get)

    def test_commit_response_must_match_pin(self):
        self.responses["/commits/" + self.sha] = {"sha": "c" * 40}
        with self.assertRaisesRegex(gate.GateError, "does not exist"):
            self.check()

    def test_commit_response_must_be_object(self):
        self.responses["/commits/" + self.sha] = []
        with self.assertRaisesRegex(gate.GateError, "does not exist"):
            self.check()

    def test_tag_must_be_remotely_available(self):
        def get(key):
            if key == "/git/ref/tags/v0.1.0-rc.1":
                raise gate.GateError("HTTP 404")
            return self.responses[key]
        with self.assertRaisesRegex(gate.GateError, "404"):
            gate.published_dependency("v0.1.0-rc.1", self.sha, get)

    def test_http_errors_fail_closed(self):
        for code in (404, 403, 429, 500):
            with self.subTest(code=code), patch.object(gate, "urlopen", side_effect=HTTPError("https://api.github.com/", code, "failure", {}, None)):
                with self.assertRaisesRegex(gate.GateError, f"HTTP {code}"):
                    gate.api_get("/commits/" + self.sha)

    def test_network_errors_fail_closed(self):
        for error in (URLError("offline"), TimeoutError(), OSError("TLS")):
            with self.subTest(error=error), patch.object(gate, "urlopen", side_effect=error):
                with self.assertRaisesRegex(gate.GateError, "cannot be skipped"):
                    gate.api_get("/commits/" + self.sha)

    def test_invalid_json_fails_closed(self):
        response = io.BytesIO(b"not JSON")
        with patch.object(gate, "urlopen", return_value=response):
            with self.assertRaisesRegex(gate.GateError, "cannot be skipped"):
                gate.api_get("/commits/" + self.sha)


if __name__ == "__main__":
    unittest.main()
