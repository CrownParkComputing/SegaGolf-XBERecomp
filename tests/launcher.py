"""Check launcher process layout, option forwarding and cleanup without a game boot."""
import importlib.util
import json
import os
from pathlib import Path
import signal
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("run_linked", Path(__file__).resolve().parents[1] / "tools/run_linked.py")
launcher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(launcher)

FAKE_GAME = '''#!/usr/bin/env python3
import json, os, signal, time
from pathlib import Path
def stop(*args):
    Path("stopped").touch()
    raise SystemExit(0)
signal.signal(signal.SIGTERM, stop)
Path("environment.json").write_text(json.dumps(dict(os.environ)))
while True:
    time.sleep(0.01)
'''


class LauncherTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.prefs = self.root / ".local/launcher.json"
        binary = self.root / "out/build/linux-release/SegaGolf"
        binary.parent.mkdir(parents=True)
        binary.write_text(FAKE_GAME)
        binary.chmod(0o755)
        for name, value in (("ROOT", self.root), ("PREFERENCES", self.prefs)):
            context = patch.object(launcher, name, value)
            context.start()
            self.addCleanup(context.stop)
        previous = signal.getsignal(signal.SIGTERM)
        self.addCleanup(signal.signal, signal.SIGTERM, previous)

    def cabinet(self, role):
        folder = self.root / ".local/linked" / role
        self.assertTrue((folder / "stopped").exists(), f"{role} was not stopped")
        return json.loads((folder / "environment.json").read_text())

    def test_one_player_japanese_overrides_inherited_english(self):
        with patch.dict(os.environ, {"SEGAGOLF_LANGUAGE": "en"}):
            self.assertEqual(launcher.main(["--players", "1", "--language", "ja", "--seconds", "1"]), 0)
        self.assertFalse((self.root / ".local/linked/sub").exists())
        for role, ip, hidden in (("server", "127.0.0.1", "1"), ("main", "127.0.0.2", "0")):
            env = self.cabinet(role)
            self.assertEqual(env["SEGAGOLF_ROLE"], role)
            self.assertEqual(env["SEGAGOLF_LOCAL_IP"], ip)
            self.assertEqual(env["SEGAGOLF_LANGUAGE"], "ja")
            self.assertEqual(env["SEGAGOLF_PLAYERS"], "1")
            self.assertEqual(env["XBE_VULKAN_OFFSCREEN"], hidden)
        self.assertEqual(json.loads(self.prefs.read_text()), {"players": 1, "language": "ja"})

    def test_saved_two_player_setup_reaches_each_cabinet(self):
        self.prefs.parent.mkdir()
        self.prefs.write_text('{"players": 2, "language": "en"}')
        with patch.dict(os.environ, {"SEGAGOLF_LANGUAGE": "en"}):
            self.assertEqual(launcher.main(["--no-menu", "--seconds", "1"]), 0)
        for index, role in enumerate(("server", "main", "sub")):
            env = self.cabinet(role)
            self.assertEqual(env["SEGAGOLF_PLAYER"], str(index))
            self.assertEqual(env["SEGAGOLF_PLAYERS"], "2")
            self.assertEqual(env["SEGAGOLF_LANGUAGE"], "en")
            self.assertEqual(env["SEGAGOLF_LOCAL_IP"], f"127.0.0.{index + 1}")

    def test_cancel_options_does_not_start_cabinets_or_change_preferences(self):
        self.prefs.parent.mkdir()
        self.prefs.write_text('{"players": 1, "language": "ja"}')
        before = self.prefs.read_bytes()
        with patch.object(launcher, "choose_options", return_value=None), patch.object(launcher.subprocess, "Popen") as popen:
            self.assertEqual(launcher.main(["--menu"]), 0)
            popen.assert_not_called()
        self.assertEqual(self.prefs.read_bytes(), before)


if __name__ == "__main__":
    unittest.main()
