# Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
"""Focused disposable-file tests; never register a live desktop or start the app."""
import importlib.util
import os
from pathlib import Path
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("registration", ROOT / "scripts/register-linux-desktop.py")
registration = importlib.util.module_from_spec(spec)
spec.loader.exec_module(registration)


@unittest.skipUnless(sys.platform.startswith("linux"), "Linux desktop integration")
class RegistrationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.binary = self.root / "Qt Creator Debug" / "RayNmeaSimulator"
        self.binary.parent.mkdir()
        self.binary.write_bytes(b"disposable binary fixture; never executed")
        self.binary.chmod(0o755)
        self.data = self.root / "user data"

    def test_identity_paths_and_exact_approved_icon(self):
        desktop, icon, backups = registration.register(self.binary, self.data)
        self.assertEqual(desktop.name, "com.rfstateside.RayNmeaSimulator.desktop")
        text = desktop.read_text(encoding="utf-8")
        self.assertIn('Exec="' + str(self.binary.resolve()) + '"', text)
        self.assertIn("TryExec=" + str(self.binary.resolve()), text)
        self.assertIn("StartupWMClass=RayNmeaSimulator", text)
        self.assertIn("Icon=" + str(icon), text)
        self.assertEqual(icon.read_bytes(), (ROOT / "images/RayNmeaSimulator_1024.png").read_bytes())
        self.assertEqual(icon.parent, self.data / "icons")
        self.assertFalse(backups)
        self.assertIn('setDesktopFileName("com.rfstateside.RayNmeaSimulator")',
                      (ROOT / "src/main.cpp").read_text(encoding="utf-8"))

    def test_repeat_is_noop_and_changes_preserve_originals(self):
        desktop, icon, _ = registration.register(self.binary, self.data)
        before = (desktop.stat().st_mtime_ns, icon.stat().st_mtime_ns)
        self.assertFalse(registration.register(self.binary, self.data)[2])
        self.assertEqual(before, (desktop.stat().st_mtime_ns, icon.stat().st_mtime_ns))
        desktop.write_bytes(b"previous user launcher")
        icon.write_bytes(b"previous user icon")
        backups = registration.register(self.binary, self.data)[2]
        self.assertEqual({p.read_bytes() for p in backups}, {b"previous user launcher", b"previous user icon"})

    def test_missing_binary_and_blocked_target_change_nothing(self):
        with self.assertRaises(OSError):
            registration.register(self.root / "missing", self.data)
        self.assertFalse(self.data.exists())
        desktop = self.data / "applications" / registration.DESKTOP_ID
        desktop.mkdir(parents=True)
        with self.assertRaises(ValueError):
            registration.register(self.binary, self.data)
        self.assertEqual(list(self.data.rglob("*.png")), [])

    def test_xdg_and_invalid_paths(self):
        with patch.dict(os.environ, {"XDG_DATA_HOME": str(self.data)}):
            self.assertEqual(registration.data_home(), self.data)
        with patch.dict(os.environ, {"XDG_DATA_HOME": ""}):
            self.assertEqual(registration.data_home(), Path.home() / ".local/share")
        with patch.dict(os.environ, {"XDG_DATA_HOME": "relative"}):
            with self.assertRaises(ValueError):
                registration.data_home()
        for name in ["/tmp/bad\nname", "/tmp/bad=name", "/tmp/bad%name"]:
            with self.assertRaises(ValueError):
                registration.desktop_entry(Path(name))

    def test_exec_special_characters_are_literal(self):
        text = registration.desktop_entry(Path('/tmp/a $b`c"d\\e'))
        line = next(x for x in text.splitlines() if x.startswith("Exec="))
        self.assertIn("\\\\$", line)
        self.assertIn("\\\\`", line)
        self.assertIn('\\\\"', line)
        self.assertIn("\\\\\\\\e", line)

    def test_gio_launch_preserves_special_path_characters(self):
        try:
            from gi.repository import GioUnix
            desktop_class = GioUnix.DesktopAppInfo
        except ImportError:
            try:
                from gi.repository import Gio
                desktop_class = Gio.DesktopAppInfo
            except ImportError:
                self.skipTest("Optional system GIO bindings unavailable")
        binary = self.root / 'Space $cash`tick"quote\\slash'
        marker = self.root / "launched.txt"
        binary.write_text("#!/usr/bin/python3\nfrom pathlib import Path\nPath(" + repr(str(marker))
                          + ").write_text('launched')\n", encoding="utf-8")
        binary.chmod(0o755)
        desktop, _, _ = registration.register(binary, self.data)
        app = desktop_class.new_from_filename(str(desktop))
        self.assertIsNotNone(app)
        self.assertTrue(app.launch([], None))
        for _ in range(40):
            if marker.exists():
                break
            time.sleep(0.05)
        self.assertEqual(marker.read_text(), "launched")


if __name__ == "__main__":
    unittest.main()
