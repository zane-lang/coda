import os
import sys

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..")))

from tests.harness.test_harness import run_catalog_tests
from bindings.python.coda import Doc, Table, KeyedTable

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


test_empty_array_serializes_multiline()
test_empty_table_and_keyed_table_keep_headers()
test_table_and_keyed_table_require_columns()
