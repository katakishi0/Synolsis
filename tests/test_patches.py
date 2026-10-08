import subprocess
import tempfile
import unittest
from pathlib import Path

from tools.verify_patches import PatchError, changed_files, verify


def git(path, *args):
    run = subprocess.run(["git", *args], cwd=path, capture_output=True, text=True, check=True)
    return run.stdout.strip()


class PatchSafetyTests(unittest.TestCase):
    def test_blocks_parent_path(self):
        with self.assertRaisesRegex(PatchError, "outside"):
            changed_files(b"diff --git a/app/../../secret b/app/../../secret\n")

    def test_blocks_renamed_path(self):
        with self.assertRaisesRegex(PatchError, "Renames"):
            changed_files(b"diff --git a/app/old b/app/new\n")

    def test_blocks_binary_payload(self):
        with self.assertRaisesRegex(PatchError, "Binary"):
            changed_files(b"diff --git a/app/file b/app/file\nGIT binary patch\n")

    def test_accepts_text_file(self):
        self.assertEqual(changed_files(b"diff --git a/app/main.cpp b/app/main.cpp\n"), ["app/main.cpp"])

    def test_worktree_dry_run_write_and_dirty_rejection(self):
        with tempfile.TemporaryDirectory() as dir_name:
            tmp = Path(dir_name)
            upstream = tmp / "source"
            upstream.mkdir()
            git(upstream, "init")
            git(upstream, "config", "user.name", "Test")
            git(upstream, "config", "user.email", "test@example.invalid")
            (upstream / "app").mkdir()
            (upstream / "app" / "build.gradle").write_text("// example\n")
            (upstream / "app" / "sample.cpp").write_text("old\n")
            git(upstream, "add", ".")
            git(upstream, "commit", "-m", "initial")
            (tmp / "patches").mkdir()
            diff = b"""diff --git a/app/sample.cpp b/app/sample.cpp
--- a/app/sample.cpp
+++ b/app/sample.cpp
@@ -1 +1 @@
-old
+new
"""
            (tmp / "patches" / "pr-252.patch").write_bytes(diff)
            result = verify(upstream, 252, root=tmp)
            self.assertEqual(result["applied"], False)
            self.assertEqual((upstream / "app/sample.cpp").read_text(), "old\n")
            self.assertEqual(verify(upstream, 252, root=tmp, write=True)["applied"], True)
            self.assertEqual((upstream / "app/sample.cpp").read_text(), "new\n")
            with self.assertRaisesRegex(PatchError, "not clean"):
                verify(upstream, 252, root=tmp, write=True)


if __name__ == "__main__":
    unittest.main()
