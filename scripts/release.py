#!/usr/bin/env python3
"""Validate a stable release tag and update/check project metadata."""

import argparse
import re
import tomllib
from pathlib import Path


def prepare(version: str, path: Path, *, check: bool = False) -> str:
	match = re.fullmatch(r"v?((?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*))", version)
	if match is None:
		raise ValueError("Use a stable version such as 3.0.1 or v3.0.1.")
	version = match[1]
	text = path.read_text()
	current = tomllib.loads(text)["project"]["version"]
	if check:
		if current != version:
			raise ValueError(f"Tag version {version} does not match pyproject.toml ({current}).")
	else:
		if tuple(map(int, version.split("."))) < tuple(map(int, current.split("."))):
			raise ValueError(f"Release version {version} is older than {current}.")
		# Limit the edit to [project], preserving all other TOML formatting.
		section = re.search(r"(?ms)^\[project\]\s*\n(.*?)(?=^\[|\Z)", text)
		if section is None:
			raise ValueError("Missing [project] section.")
		updated, count = re.subn(r'^version\s*=\s*"[^"]*"', f'version = "{version}"', section[1], flags=re.M)
		if count != 1:
			raise ValueError("Expected exactly one project version assignment.")
		path.write_text(text[:section.start(1)] + updated + text[section.end(1):])
	return f"v{version}"


if __name__ == "__main__":
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("version")
	parser.add_argument("--check", action="store_true")
	args = parser.parse_args()
	try:
		print(prepare(args.version, Path("pyproject.toml"), check=args.check))
	except ValueError as error:
		parser.exit(1, f"{error}\n")
