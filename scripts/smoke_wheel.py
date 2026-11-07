#!/usr/bin/env python3
"""Check an installed coda-format wheel on the system it was built for.

Run it with the interpreter the wheel is installed in, from outside the
repository, so `import coda` finds the installed package and not the source
checkout. It fails unless the native library loaded is the one inside the
installed package, and unless that library parses and reads a document.
"""

from __future__ import annotations

import os
import sys

import coda
from coda import coda as implementation

SOURCE = "name smoke\nflags [\n\tfast\n\tsafe\n]\n"


def main() -> int:
	package = os.path.dirname(os.path.abspath(coda.__file__))
	library = os.path.abspath(implementation._lib_path)
	if os.path.dirname(library) != package:
		print(f"loaded {library}, not the wheel's library in {package}", file=sys.stderr)
		return 1
	with coda.Doc.parse(SOURCE, filename="smoke.coda") as doc:
		root = doc.root()
		name = root["name"].as_string().value
		flags = [node.as_string().value for node in root["flags"].as_array()]
	if name != "smoke" or flags != ["fast", "safe"]:
		print(f"read name={name!r} flags={flags!r}", file=sys.stderr)
		return 1
	print(f"ok: {library}, ABI {coda.get_abi_version()}")
	return 0


if __name__ == "__main__":
	sys.exit(main())
