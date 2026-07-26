import os
import sys

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))
os.environ.setdefault(
	"CODA_FFI_LIB",
	os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "build", "libcoda_ffi.so")),
)

import bindings.python as coda


def expect_error(fn, message: str) -> None:
	try:
		fn()
	except coda.Error:
		return
	raise AssertionError(message)


def main() -> None:
	assert coda.parse_error_code_name(2) == "DuplicateKey"

	with coda.Doc.new() as left, coda.Doc.new() as right:
		foreign = coda.Block()
		left.root()["foreign"] = foreign
		expect_error(lambda: right.root().insert("foreign", foreign), "cross-document insertion succeeded")

	with coda.Doc.new() as doc:
		root = doc.root()
		child = coda.Block()
		root["first"] = child
		expect_error(lambda: root.insert("second", child), "double-parent insertion succeeded")
		expect_error(lambda: child.insert("self", child), "cycle insertion succeeded")

	with coda.Doc.parse("z last\na first\n") as doc:
		root = doc.root()
		z = root["z"]
		doc.order()
		assert str(z) == "last"

	with coda.Doc.parse("table [\n z a\n]\n") as doc:
		table = doc.root()["table"].as_table()
		assert table.columns() == ["z", "a"]
		assert "\n\tz a\n" in doc.serialize()

	with coda.Doc.new() as doc:
		root = doc.root()
		root["value"] = "old"
		stale = root["value"]
		del root["value"]
		expect_error(lambda: str(stale), "stale node did not fail")
		root["value"] = "new"
		expect_error(lambda: str(stale), "stale node aliased a recycled node")

	with coda.Doc.new() as doc:
		root = doc.root()
		root["table"] = coda.KeyedTable(["value"])
		table = root["table"].as_keyed_table()
		for key in ("z", "a"):
			row = coda.Row()
			row["value"] = key
			table[key] = row
		table.order()
		assert [key for key, _ in table] == ["a", "z"]
		expect_error(lambda: table.append_col("later"), "column append invalidated existing rows")
		expect_error(lambda: table["z"].insert("unknown", "x"), "attached row accepted an unknown field")
		expect_error(lambda: table["z"].__delitem__("value"), "attached row removed a required field")

	print("Python safety tests passed")


if __name__ == "__main__":
	main()
