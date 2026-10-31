#!/usr/bin/env python3
"""Isolated CLI privacy/locking/preference checks; no accounts, keychain or network."""
import configparser
from contextlib import contextmanager
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

    @contextmanager
    def running_app(self, *arguments):
        process = subprocess.Popen([BINARY, *map(str, arguments)], env=self.env,
                                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            yield process
        finally:
            if process.poll() is None:
                process.send_signal(signal.SIGTERM)
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=5)

    def wait_for_preferences(self, process, path, expected):
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            self.assertIsNone(process.poll(), "instance exited before serializing window preferences")
            preferences = configparser.ConfigParser(interpolation=None)
            preferences.read(path, encoding="utf-8")
            if preferences.has_section("General"):
                values = preferences["General"]
                if (all(values.get(key) == value for key, value in expected.items())
                        and path.stat().st_mode & 0o777 == 0o600):
                    return values
            time.sleep(0.05)
        self.fail(f"window preferences were not serialized as {expected!r}")

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

    def test_window_summary_preference_default_and_restore_serialization(self):
        mailbox = self.root / "window-preferences"
        mailbox.mkdir(mode=0o700)
        window_ini = mailbox / "window.ini"
        # Each process opens only this new, account-free fixture. Numeric INI
        # inputs must be restored into the QML bool and saved back as true/false;
        # merely leaving the seeded bytes untouched cannot satisfy the assertions.
        for stored, expected in ((None, "true"), ("0", "false"), ("1", "true")):
            with self.subTest(stored=stored):
                if stored is not None:
                    window_ini.write_text(f"[General]\nshowSummary={stored}\n", encoding="utf-8")
                with self.running_app("--data-dir", mailbox) as process:
                    values = self.wait_for_preferences(process, window_ini, {
                        "showSummary": expected, "sidebarWidth": "236", "listWidth": "320"
                    })
                    self.assertNotIn("unreadOnly", values)
                    self.assertEqual(window_ini.stat().st_mode & 0o777, 0o600)

        # Restart from the actual serialized false value, not a hand-written
        # substitute. Removing one existing key forces an observable rewrite
        # while retaining showSummary from the previous saved file.
        window_ini.write_text("[General]\nshowSummary=0\n", encoding="utf-8")
        with self.running_app("--data-dir", mailbox) as process:
            self.wait_for_preferences(process, window_ini, {"showSummary": "false", "listWidth": "320"})
        serialized = window_ini.read_text(encoding="utf-8")
        window_ini.write_text(serialized.replace("listWidth=320\n", ""), encoding="utf-8")
        with self.running_app("--data-dir", mailbox) as process:
            values = self.wait_for_preferences(process, window_ini, {"showSummary": "false", "listWidth": "320"})
            self.assertNotIn("unreadOnly", values)

    def test_screenshot_does_not_write_window_preferences(self):
        mailbox = self.root / "existing-mailbox"
        mailbox.mkdir(mode=0o700)
        window_ini = mailbox / "window.ini"
        saved = b"[General]\nshowSummary=false\nsidebarWidth=291\nlistWidth=411\n"
        window_ini.write_bytes(saved)
        screenshot = self.root / "synthetic.png"
        temporary = Path(self.env["TMPDIR"])
        saw_demo_storage = False
        # Observe the private demo directory while the capture process is alive:
        # checking only after exit would miss preferences deleted with that dir.
        with self.running_app("--demo", "--data-dir", mailbox, "--screenshot", screenshot) as process:
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                saw_demo_storage |= any(temporary.rglob("mail.sqlite"))
                self.assertEqual(list(temporary.rglob("window.ini")), [])
                if process.poll() is not None:
                    break
                time.sleep(0.05)
            self.assertIsNotNone(process.poll(), "screenshot process did not exit")
            self.assertEqual(process.returncode, 0)
        self.assertTrue(saw_demo_storage, "did not observe isolated synthetic storage")
        self.assertEqual(window_ini.read_bytes(), saved)
        self.assertEqual(sorted(p.name for p in mailbox.iterdir()), [window_ini.name])
        self.assertTrue(screenshot.read_bytes().startswith(b"\x89PNG\r\n\x1a\n"))

    def test_demo_preferences_stay_in_private_storage(self):
        mailbox = self.root / "must-not-open"
        temporary = Path(self.env["TMPDIR"])
        with self.running_app("--demo", "--data-dir", mailbox) as process:
            deadline = time.monotonic() + 10
            window_ini = None
            while time.monotonic() < deadline:
                self.assertIsNone(process.poll(), "demo instance exited before saving preferences")
                candidates = list(temporary.rglob("window.ini"))
                if candidates:
                    self.assertEqual(len(candidates), 1)
                    window_ini = candidates[0]
                    break
                time.sleep(0.05)
            self.assertIsNotNone(window_ini, "demo did not save preferences in private storage")
            self.wait_for_preferences(process, window_ini, {"showSummary": "true"})
            self.assertFalse(mailbox.exists())
            self.assertFalse(list(Path(self.env["XDG_CONFIG_HOME"]).rglob("window.ini")))
            self.assertFalse(list(Path(self.env["XDG_DATA_HOME"]).rglob("window.ini")))

    def test_mailbox_root_is_private_and_single_instance(self):
        mailbox = self.root / "empty-mailbox"
        with self.running_app("--data-dir", mailbox) as first:
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


if __name__ == "__main__":
    unittest.main()
