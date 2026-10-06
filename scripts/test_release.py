"""Tests for release input validation and metadata updates."""

import tempfile
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


if __name__ == "__main__":
	unittest.main()
