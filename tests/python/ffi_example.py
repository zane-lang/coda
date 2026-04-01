import os
import sys

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))

from bindings.python.coda import Coda, CodaDoc

text = """\
name example
type executable

deps [
	key link version
	plot github.com/zane-lang/plot 4.0.3
	http github.com/zane-lang/http 2.1.0
]

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
	print(doc["name"].asString())
	print(doc["compiler"]["debug"].asString())
	print(doc["deps"]["plot"]["link"].asString())
	for target in doc["compiler"]["targets"].asArray():
		print(target.asString())
	for key, value in doc["meta"].asBlock():
		print(f"{key}: {value.asString()}")
	print(doc.serialize())
