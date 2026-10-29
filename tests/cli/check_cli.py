#!/usr/bin/env python3
"""Isolated CLI privacy/locking checks; no accounts, keychain or network."""
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import unittest

BINARY = str(Path(sys.argv.pop(1)).resolve())


class CliTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="nkmail-cli-")
        self.root = Path(self.temporary.name)
        self.env = os.environ.copy()
        self.env.update(QT_QPA_PLATFORM="offscreen", QT_QUICK_BACKEND="software",
                        QT_FORCE_STDERR_LOGGING="1")
        for variable in ("XDG_CONFIG_HOME", "XDG_DATA_HOME", "XDG_CACHE_HOME", "TMPDIR"):
            directory = self.root / variable.lower()
            directory.mkdir(mode=0o700)
            self.env[variable] = str(directory)

    def tearDown(self):
        self.temporary.cleanup()

    def run_app(self, *arguments):
        return subprocess.run([BINARY, *map(str, arguments)], env=self.env,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              timeout=20)

    def test_screenshot_requires_demo_before_opening_mailbox(self):
        mailbox = self.root / "must-not-open"
        screenshot = self.root / "forbidden.png"
        result = self.run_app("--data-dir", mailbox, "--screenshot", screenshot)
        self.assertEqual(result.returncode, 2)
        self.assertFalse(mailbox.exists())
        self.assertFalse(screenshot.exists())

    def test_demo_ignores_persistent_mailbox_root(self):
        mailbox = self.root / "existing-mailbox"
        mailbox.mkdir(mode=0o700)
        sentinel = mailbox / "private-sentinel"
        sentinel.write_bytes(b"must remain untouched")
        screenshot = self.root / "synthetic.png"
        result = self.run_app("--demo", "--data-dir", mailbox, "--screenshot", screenshot)
        self.assertEqual(result.returncode, 0, result.stderr.decode(errors="replace"))
        self.assertEqual(sorted(p.name for p in mailbox.iterdir()), [sentinel.name])
        self.assertEqual(sentinel.read_bytes(), b"must remain untouched")
        self.assertTrue(screenshot.read_bytes().startswith(b"\x89PNG\r\n\x1a\n"))

    def test_mailbox_root_is_private_and_single_instance(self):
        mailbox = self.root / "empty-mailbox"
        first = subprocess.Popen([BINARY, "--data-dir", str(mailbox)], env=self.env,
                                 stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            deadline = time.monotonic() + 10
            database = mailbox / "mail.sqlite"
            while time.monotonic() < deadline and not database.exists():
                self.assertIsNone(first.poll(), "first instance exited before opening storage")
                time.sleep(0.05)
            self.assertTrue(database.exists(), "first instance did not initialize storage")
            result = self.run_app("--data-dir", mailbox)
            self.assertEqual(result.returncode, 2)
            self.assertIn(b"already open", result.stderr)
            self.assertEqual(mailbox.stat().st_mode & 0o777, 0o700)
            self.assertEqual(database.stat().st_mode & 0o777, 0o600)
        finally:
            if first.poll() is None:
                first.send_signal(signal.SIGTERM)
            try:
                first.wait(timeout=5)
            except subprocess.TimeoutExpired:
                first.kill()
                first.wait(timeout=5)


if __name__ == "__main__":
    unittest.main()
