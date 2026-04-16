import os
import sys

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))

from tests.harness.test_harness import run_catalog_tests
from bindings.python.coda import Doc, Table, KeyedTable, Row

catalog_path = os.path.join(os.path.dirname(__file__), "..", "catalog", "catalog.coda")
run_catalog_tests(os.path.abspath(catalog_path))


def test_empty_array_serializes_multiline() -> None:
	with Doc.parse("empty-array []\n") as doc:
		serialized = doc.serialize()
		assert "empty-array [\n]" in serialized


def test_empty_table_and_keyed_table_keep_headers() -> None:
	with Doc.new() as doc:
		root = doc.root()
		root.insert("rows", Table(["name", "version"]))
		root.insert("deps", KeyedTable(["link", "version"]))

		serialized = doc.serialize()
		assert "rows [\n\tname version\n]" in serialized
		assert "deps [\n\tkey link version\n]" in serialized

	with Doc.parse(serialized) as parsed:
		assert parsed.serialize() == serialized


def test_table_and_keyed_table_require_columns() -> None:
	try:
		Table()
		raise AssertionError("Table() should require columns argument")
	except TypeError:
		pass

	try:
		KeyedTable()
		raise AssertionError("KeyedTable() should require columns argument")
	except TypeError:
		pass

	try:
		Table([])
		raise AssertionError("Table([]) should be rejected")
	except ValueError:
		pass

	try:
		KeyedTable([])
		raise AssertionError("KeyedTable([]) should be rejected")
	except ValueError:
		pass

def test_row_insert_chaining() -> None:
	with Doc.new() as doc:
		root = doc.root()
		root.insert("rows", Table(["name", "version"]))
		root["rows"].as_table().append(
			Row().insert("name", "coda").insert("version", "2.2.2")
		)
		row = root["rows"].as_table()[0]
		assert row["name"] == "coda"
		assert row["version"] == "2.2.2"


def test_block_has_matches_membership() -> None:
	with Doc.parse("a 1\n") as doc:
		root = doc.root()
		assert root.has("a")
		assert not root.has("missing")
		assert root.has("a") == ("a" in root)
		assert root.has("missing") == ("missing" in root)


test_empty_array_serializes_multiline()
test_empty_table_and_keyed_table_keep_headers()
test_table_and_keyed_table_require_columns()
test_row_insert_chaining()
test_block_has_matches_membership()
