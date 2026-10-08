import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import project
from tools.gpu_report import identify_gpu


def g(*args, cwd):
    p = subprocess.run(["git", *args], cwd=cwd, capture_output=True, text=True)
    if p.returncode:
        raise AssertionError(f"git {' '.join(args)} failed: {p.stderr}")
    return p.stdout.strip()


class ProjectIntegrationTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.remote = self.root / "source"
        self.remote.mkdir()
        g("init", "-b", "master", cwd=self.remote)
        g("config", "user.email", "ci@example.invalid", cwd=self.remote)
        g("config", "user.name", "OfflineTest", cwd=self.remote)
        (self.remote / "app").mkdir()
        (self.remote / "app" / "build.gradle").write_text("// offline test\n")
        (self.remote / "test.txt").write_text("original\n")
        g("add", ".", cwd=self.remote)
        g("commit", "-m", "base", cwd=self.remote)
        self.base_sha = g("rev-parse", "HEAD", cwd=self.remote)
        g("branch", "base", cwd=self.remote)
        (self.remote / "test.txt").write_text("fixed\n")
        g("add", ".", cwd=self.remote)
        g("commit", "-m", "test patch", cwd=self.remote)
        self.patch_sha = g("rev-parse", "HEAD", cwd=self.remote)
        g("update-ref", "refs/pull/252/head", self.patch_sha, cwd=self.remote)
        self.work = self.root / "work"
        g("clone", "-b", "base", str(self.remote), str(self.work), cwd=self.root)
        g("config", "user.email", "ci@example.invalid", cwd=self.work)
        g("config", "user.name", "OfflineTest", cwd=self.work)
        self.p1 = patch.dict(project.CATALOG, {"upstream": str(self.remote)})
        self.p2 = patch.dict(project.PATCHES[252], {"head_sha": self.patch_sha})
        self.p1.start()
        self.p2.start()

    def tearDown(self):
        self.p2.stop()
        self.p1.stop()
        self.tmp.cleanup()

    def test_dry_run_does_not_modify_tree(self):
        project.apply_patch(self.work, 252, write=False)
        self.assertEqual((self.work / "test.txt").read_text(), "original\n")
        self.assertEqual(g("branch", "--show-current", cwd=self.work), "base")
        self.assertEqual(g("status", "--porcelain", cwd=self.work), "")

    def test_apply_creates_new_branch_and_commit(self):
        project.apply_patch(self.work, 252, write=True)
        self.assertEqual((self.work / "test.txt").read_text(), "fixed\n")
        self.assertTrue(g("branch", "--show-current", cwd=self.work).startswith("fix/pr-252-"))
        self.assertIn("Original author: PabloG02", g("show", "-s", "--format=%B", cwd=self.work))
        self.assertEqual(g("status", "--porcelain", cwd=self.work), "")

    def test_dirty_tree_rejected(self):
        (self.work / "private.txt").write_text("don't touch")
        with self.assertRaisesRegex(project.ProjectError, "alterações não commitadas"):
            project.apply_patch(self.work, 252, write=True)
        self.assertEqual((self.work / "test.txt").read_text(), "original\n")

    def test_pr_sha_mutation_rejected(self):
        with patch.dict(project.PATCHES[252], {"head_sha": "a" * 40}):
            with self.assertRaisesRegex(project.ProjectError, "foi atualizado"):
                project.apply_patch(self.work, 252, write=True)
        self.assertEqual(g("branch", "--show-current", cwd=self.work), "base")

    def test_research_only_rejected(self):
        with self.assertRaisesRegex(project.ProjectError, "pesquisa bloqueado"):
            project.apply_patch(self.work, 298, write=True)


class GpuTests(unittest.TestCase):
    def test_powervr(self):
        self.assertEqual(identify_gpu("PowerVR BXM-8-256"), "PowerVR / Imagination")

    def test_mali(self):
        self.assertEqual(identify_gpu("Mali G615 Vulkan"), "ARM Mali")

    def test_adreno(self):
        self.assertEqual(identify_gpu("Qualcomm Adreno 740"), "Qualcomm Adreno")

    def test_unknown(self):
        self.assertIn("Desconhecida", identify_gpu("other GPU"))


if __name__ == "__main__":
    unittest.main()
