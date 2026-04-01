from coda_ffi import CodaDoc

text = """\
name test
compiler {
	debug false
}
"""

with CodaDoc.parse(text) as doc:
	print(doc["compiler"]["debug"])
	print(doc.serialize())
