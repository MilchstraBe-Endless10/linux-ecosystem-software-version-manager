import json
from pathlib import Path
import tempfile
import unittest
from inventory import discover, _icon_info


class InventoryTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        root = Path(self.tmp.name)
        self.pacman = root / "local"
        package = self.pacman / "example-1.2-1"
        package.mkdir(parents=True)
        (package / "desc").write_text("%NAME%\nexample\n\n%VERSION%\n1.2-1\n\n%DESC%\nExample\n")
        (package / "files").write_text("%FILES%\nusr/share/applications/example.desktop\n")
        self.data = root / "share"
        (self.data / "applications").mkdir(parents=True)
        (self.data / "applications/example.desktop").write_text("[Desktop Entry]\nType=Application\nName=Example\nIcon=example-icon\nExec=/bin/false\n")

    def tearDown(self):
        self.tmp.cleanup()

    def test_real_metadata_no_fake_count(self):
        out = discover(self.pacman, [(self.data, "xdg-system")])
        self.assertEqual(out["counts"], {"pacman_packages": 1, "desktop_launchers": 1})
        self.assertEqual(out["packages"][0]["installed_version"], "1.2-1")
        self.assertIsNone(out["packages"][0]["latest_version"])
        self.assertEqual(out["packages"][0]["update_status"], "unknown")

    def test_desktop_icon_and_ownership(self):
        app = discover(self.pacman, [(self.data, "xdg-system")])["launchers"][0]
        self.assertEqual(app["icon_name"], "example-icon")
        self.assertEqual(app["package_id"], "example")
        self.assertEqual(app["manager"], "pacman")
        self.assertEqual(app["icon_provenance"], "desktop-entry-theme")

    def test_do_not_execute_desktop_exec(self):
        out = discover(self.pacman, [(self.data, "xdg-system")])
        self.assertNotIn("Exec", json.dumps(out))
        self.assertNotIn("/bin/false", json.dumps(out))

    def test_absolute_icon_path_not_trusted(self):
        path = self.data / "applications/example.desktop"
        path.write_text("[Desktop Entry]\nName=Example\nIcon=/home/user/secret.png\n")
        app = discover(self.pacman, [(self.data, "xdg-system")])["launchers"][0]
        self.assertIsNone(app["icon_path"])
        self.assertIsNone(app["icon_name"])
        self.assertEqual(app["icon_provenance"], "absolute-path-requires-verification")

    def test_hidden_excluded(self):
        p = self.data / "applications/example.desktop"
        p.write_text("[Desktop Entry]\nName=Example\nHidden=true\nIcon=example\n")
        self.assertEqual(discover(self.pacman, [(self.data, "xdg-system")])["counts"]["desktop_launchers"], 0)

    def test_hidden_user_entry_masks_system_entry(self):
        user = Path(self.tmp.name) / "user-share"
        (user / "applications").mkdir(parents=True)
        (user / "applications/example.desktop").write_text("[Desktop Entry]\nName=Example\nHidden=true\n")
        result = discover(self.pacman, [(user, "xdg-user"), (self.data, "xdg-system")])
        self.assertEqual(result["counts"]["desktop_launchers"], 0)

    def test_unreadable_or_malformed_warns(self):
        (self.data / "applications/bad.desktop").write_text("Invalid\ncontent")
        self.assertTrue(discover(self.pacman, [(self.data, "xdg-system")])["warnings"])

    def test_desktop_symlink_skipped(self):
        (self.data / "applications/link.desktop").symlink_to("example.desktop")
        self.assertEqual(discover(self.pacman, [(self.data, "xdg-system")])["counts"]["desktop_launchers"], 1)

    def test_icon_token_rejected(self):
        self.assertEqual(_icon_info("../../foo")[2], "invalid-desktop-icon")
        self.assertEqual(_icon_info("")[2], "missing")

    def test_missing_database_reports_partial_coverage(self):
        result = discover(self.pacman / "missing", [(self.data, "xdg-system")])
        self.assertEqual(result["counts"]["pacman_packages"], 0)
        self.assertTrue(result["warnings"])
        self.assertEqual(result["counts"]["desktop_launchers"], 1)


if __name__ == "__main__":
    unittest.main()
