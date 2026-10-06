"""Tests for release input validation and metadata updates."""

import tempfile
import subprocess
import sys
import unittest
from pathlib import Path

from release import prepare


class ReleaseTests(unittest.TestCase):
	def setUp(self):
		self.directory = tempfile.TemporaryDirectory()
		self.addCleanup(self.directory.cleanup)
		self.path = Path(self.directory.name) / "pyproject.toml"
		self.original = '[project]\nname = "coda-format"\nversion = "3.0.0" # keep comment\n\n[tool.example]\nversion = "other"\n'
		self.path.write_text(self.original)

	def test_version_with_and_without_prefix(self):
		for version in ("3.0.1", "v3.0.1"):
			with self.subTest(version=version):
				self.path.write_text(self.original)
				self.assertEqual(prepare(version, self.path), "v3.0.1")
				self.assertEqual(self.path.read_text(), self.original.replace('"3.0.0"', '"3.0.1"'))

	def test_invalid_inputs_leave_metadata_unchanged(self):
		for version in ("", " v3.0.1", "3.0", "3.0.1rc1", "3.00.1", "-1.0.0", "3.0.1\n", "3.0.1; touch /tmp/injected", "$(whoami)"):
			with self.subTest(version=version):
				with self.assertRaises(ValueError):
					prepare(version, self.path)
				self.assertEqual(self.path.read_text(), self.original)

	def test_versions_compare_numerically(self):
		self.assertEqual(prepare("3.0.10", self.path), "v3.0.10")
		with self.assertRaises(ValueError):
			prepare("3.0.9", self.path)
		self.assertIn('version = "3.0.10"', self.path.read_text())

	def test_existing_version_can_be_tagged(self):
		self.assertEqual(prepare("3.0.0", self.path), "v3.0.0")
		self.assertEqual(self.path.read_text(), self.original)

	def test_tag_check_does_not_modify_metadata(self):
		self.assertEqual(prepare("v3.0.0", self.path, check=True), "v3.0.0")
		with self.assertRaises(ValueError):
			prepare("v3.0.1", self.path, check=True)
		self.assertEqual(self.path.read_text(), self.original)

	def test_invalid_metadata_fails_without_modification(self):
		cases = (
			'[project\nversion = "3.0.0"\n',
			'[tool.example]\nversion = "3.0.0"\n',
			'[project]\nname = "coda-format"\n',
			'project = "not a table"\n',
			'[project]\nversion = 3\n',
			'[project]\nversion = ["3.0.0"]\n',
			'[project]\nversion = "3.0.0rc1"\n',
			'[project]\nversion = "v3.0.0"\n',
			'[project]\nversion = "3.0"\n',
			'[project]\nversion = "3.00.0"\n',
		)
		for text in cases:
			for check in (False, True):
				with self.subTest(text=text, check=check):
					self.path.write_text(text)
					with self.assertRaisesRegex(ValueError, "pyproject.toml"):
						prepare("3.0.1", self.path, check=check)
					self.assertEqual(self.path.read_text(), text)
				command = [sys.executable, str(Path(__file__).with_name("release.py").resolve()), "3.0.1"]
				if check:
					command.append("--check")
				result = subprocess.run(command, cwd=self.path.parent, capture_output=True, text=True)
				self.assertEqual(result.returncode, 1)
				self.assertIn("pyproject.toml", result.stderr)
				self.assertNotIn("Traceback", result.stderr)
				self.assertEqual(result.stdout, "")
				self.assertEqual(self.path.read_text(), text)


if __name__ == "__main__":
	unittest.main()
