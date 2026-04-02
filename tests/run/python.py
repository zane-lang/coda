import os
import sys

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))

from bindings.python.coda import Coda, CodaDoc

text = """\
name example
type executable

deps [
	# possible
	a link version
	plot github.com/zane-lang/plot 4.0.3
	http github.com/zane-lang/http 2.1.0
]

# comp
compiler {
	debug false
	optimize true
	targets [
		x86_64-linux
		x86_64-windows
		aarch64-macos
	]
}

meta {
	author "Albert Einstein"
	description "a test project"
}
"""

with CodaDoc.parse(text) as doc:
	print(doc["deps"][0]["a"].as_string())
