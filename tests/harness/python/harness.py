"""
Python test harness for catalog-driven tests.

Mirrors the C++ harness in tests/harness/ — contains CodaTestRunner and
run_catalog_tests so that bindings/python/coda.py stays focused on the
FFI bindings.
"""

import os
import sys

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "..")))

from bindings.python.coda import (
	Doc,
	Block,
	Array,
	Table,
	KeyedTable,
	Row,
	Node,
	Error,
	ParseError,
)

from typing import Optional

# ─── ANSI colours ─────────────────────────────────────────────────────────────

_ansi_green  = "\033[32m"
_ansi_red    = "\033[31m"
_ansi_yellow = "\033[33m"
_ansi_reset  = "\033[0m"


# ─── Test runner ──────────────────────────────────────────────────────────────

class CodaTestRunner:
	"""Executes catalog-driven tests against a Doc."""

	def __init__(self, doc: Doc):
		self.doc  = doc
		self.root = doc.root()

	def _bool(self, v: str) -> bool:   return v in ("true", "1", "yes")
	def _int(self,  v: str) -> int:    return int(v)
	def _float(self, v: str) -> float: return float(v)

	def _strings(self, node: Node) -> list[str]:
		return [str(v) for v in node.as_array()]

	def _order_contains(self, text: str, order: list[str]) -> bool:
		pos = 0
		for needle in order:
			found = text.find(needle, pos)
			if found < 0:
				return False
			pos = found + len(needle)
		return True

	def _path_walk(self, start_key: str, keys: list[str]) -> Node:
		"""Walk a path of keys, returning the final node."""
		node: Node = self.root[start_key]
		for key in keys:
			node = node.as_block()[key]
		return node

	def _try_key(self, key: str) -> bool:
		"""Return True if key exists in file, False otherwise."""
		try:
			_ = self.root[key]
			return True
		except KeyError:
			return False

	def _try_access(self, fn) -> bool:
		"""Execute fn and return True; catch exceptions and return False."""
		try:
			fn()
			return False
		except (TypeError, IndexError, KeyError, Error):
			return True

	def _get_header_comment(self, node: Node) -> Optional[str]:
		"""Get header_comment if node supports it."""
		if isinstance(node, Array):
			return node.header_comment
		if isinstance(node, Table):
			return node.header_comment
		if isinstance(node, KeyedTable):
			return node.header_comment
		return None

	def run_check(self, check: Block) -> bool:
		op = str(check["op"])

		if op == "get_string":
			return str(self.root[str(check["field"])]) == str(check["eq"])

		if op == "get_string_path":
			path = self._strings(check["path"])
			node = self._path_walk(path[0], path[1:])
			return str(node) == str(check["eq"])

		if op == "is_container":
			return self.root[str(check["field"])].is_container() == self._bool(str(check["eq_bool"]))

		if op == "has_key":
			got = self._try_key(str(check["field"]))
			return got == self._bool(str(check["eq_bool"]))

		if op == "map_len":
			return len(self.root[str(check["field"])].as_block()) == self._int(str(check["eq_int"]))

		if op == "map_keys":
			keys = [k for k, _ in self.root[str(check["field"])].as_block()]
			return keys == self._strings(check["eq_list"])

		if op == "array_len":
			return len(self.root[str(check["field"])].as_array()) == self._int(str(check["eq_int"]))

		if op == "array_element":
			arr = self.root[str(check["field"])].as_array()
			return str(arr[self._int(str(check["idx"]))]) == str(check["eq"])

		if op == "array_block_count":
			return len(self.root[str(check["field"])].as_array()) == self._int(str(check["eq_int"]))

		if op == "array_block_field":
			arr   = self.root[str(check["field"])].as_array()
			idx   = self._int(str(check["idx"]))
			field = str(check["field_name"])
			return str(arr[idx].as_block()[field]) == str(check["eq"])

		if op == "array_index_throws":
			arr = self.root[str(check["field"])].as_array()
			got = self._try_access(lambda: arr[self._int(str(check["idx"]))])
			return got == self._bool(str(check["eq_bool"]))

		if op == "plain_table_cell":
			ptable = self.root[str(check["table"])].as_table()
			return ptable[self._int(str(check["idx"]))][str(check["col"])] == str(check["eq"])

		if op == "table_cell":
			kt = self.root[str(check["table"])].as_keyed_table()
			return kt[str(check["row"])][str(check["col"])] == str(check["eq"])

		if op == "table_row_keys":
			keys = [k for k, _ in self.root[str(check["table"])].as_keyed_table()]
			return keys == self._strings(check["eq_list"])

		if op == "table_row_missing_inserts":
			kt = self.root[str(check["table"])].as_keyed_table()
			try:
				row_key = str(check["row"])
				kt.insert(row_key, Row())
				row_exists = row_key in kt
				row_is_empty = len(kt[row_key]) == 0 if row_exists else False
				got = row_exists and row_is_empty
			except Exception:
				got = False
			return got == self._bool(str(check["eq_bool"]))

		if op == "table_row_missing_throws":
			kt = self.root[str(check["table"])].as_keyed_table()
			got = self._try_access(lambda: kt[str(check["row"])])
			return got == self._bool(str(check["eq_bool"]))

		if op == "comment":
			return self.root[str(check["field"])].comment == str(check["eq"])

		if op == "header_comment":
			node = self.root[str(check["field"])]
			hc = self._get_header_comment(node)
			return (hc == str(check["eq"])) if hc is not None else False

		if op == "comment_path":
			path = self._strings(check["path"])
			node = self._path_walk(path[0], path[1:])
			return node.comment == str(check["eq"])

		if op == "array_element_comment":
			arr = self.root[str(check["field"])].as_array()
			return arr[self._int(str(check["idx"]))].comment == str(check["eq"])

		if op == "table_row_comment":
			kt = self.root[str(check["table"])].as_keyed_table()
			return kt[str(check["row"])].comment == str(check["eq"])

		if op == "plain_table_row_comment":
			ptable = self.root[str(check["table"])].as_table()
			return ptable[self._int(str(check["idx"]))].comment == str(check["eq"])

		if op == "set_string":
			key = str(check["field"])
			val = str(check["value"])
			self.root.insert(key, val)
			return str(self.root[key]) == val

		if op == "set_string_path":
			path = self._strings(check["path"])
			block = self.root[path[0]].as_block()
			for key in path[1:-1]:
				block = block[key].as_block()
			val = str(check["value"])
			block.insert(path[-1], val)
			return str(block[path[-1]]) == val

		if op == "string_index_on_scalar_throws":
			got = self._try_access(lambda: self.root[str(check["field"])].as_block()[str(check["sub"])])
			return got == self._bool(str(check["eq_bool"]))

		if op == "int_index_on_block_throws":
			got = self._try_access(lambda: self.root[str(check["field"])].as_array()[self._int(str(check["idx"]))])
			return got == self._bool(str(check["eq_bool"]))

		if op == "as_array_on_scalar_throws":
			node = self.root[str(check["field"])]
			got  = not isinstance(node, Array)
			return got == self._bool(str(check["eq_bool"]))

		if op == "as_block_on_array_throws":
			node = self.root[str(check["field"])]
			got  = not isinstance(node, Block)
			return got == self._bool(str(check["eq_bool"]))

		if op == "as_table_on_block_throws":
			node = self.root[str(check["field"])]
			got  = not isinstance(node, (Table, KeyedTable))
			return got == self._bool(str(check["eq_bool"]))

		if op == "const_missing_key_throws":
			got = self._try_access(lambda: self.root[str(check["field"])])
			return got == self._bool(str(check["eq_bool"]))

		if op == "order_default_contains_order":
			order = self._strings(check["order"])
			self.doc.order()
			return self._order_contains(self.doc.serialize(), order)

		if op == "order_weighted_contains_order":
			order   = self._strings(check["order"])
			weights = [
				(str(entry.as_block()["field"]), self._float(str(entry.as_block()["weight"])))
				for entry in check["weights"].as_array()
			]
			self.doc.order_weighted(weights)
			return self._order_contains(
				self.doc.serialize(), order
			)

		if op == "serialize_contains":
			return str(check["contains"]) in self.doc.serialize(str(check["indent"]))

		return False


def run_catalog_tests(catalog_path: str) -> None:
	def test_parse_fail_msg(src: str, test: Block) -> bool:
		try:
			Doc.parse(src)
		except ParseError as e:
			needles = [str(v) for v in test["needles"].as_array()]
			return any(n in str(e) for n in needles) or not needles
		return False

	def test_parse_fail_code(src: str, test: Block) -> bool:
		code_map = {
			"UnexpectedToken":    0, "UnexpectedEOF":      1,
			"DuplicateKey":       2, "DuplicateField":     3,
			"RaggedRow":          4, "InvalidEscape":      5,
			"UnterminatedString": 6, "NestedBlock":        7,
			"ContentAfterBrace":  8, "KeyInBlock":         9,
		}
		try:
			Doc.parse(src)
		except ParseError as e:
			return int(e.code) == code_map.get(str(test["code"]), -1)
		return False

	def test_roundtrip(src: str, test: Block) -> bool:
		with Doc.parse(src) as d1:
			s1 = d1.serialize()
		with Doc.parse(s1) as d2:
			s2 = d2.serialize()
		return s1 == s2

	def test_check_all(src: str, test: Block) -> bool:
		with Doc.parse(src) as doc:
			runner = CodaTestRunner(doc)
			try:
				checks = list(doc.root()["checks"].as_array())
			except KeyError:
				checks = []
			for check_node in checks:
				if not runner.run_check(check_node.as_block()):
					return False
		return True

	with open(catalog_path, "r", encoding="utf-8") as f:
		catalog_text = f.read()

	with Doc.parse(catalog_text) as catalog_doc:
		catalog = catalog_doc.root()
		tests   = list(catalog["tests"].as_array())
		passed  = 0
		failed  = 0
		current_suite = None

		for test_node in tests:
			test  = test_node.as_block()
			suite = str(test["suite"])
			name  = str(test["name"])
			src   = str(test["src"])

			if suite != current_suite:
				current_suite = suite
				print(f"\n{_ansi_yellow}[{suite}]{_ansi_reset}")

			action = None
			try:
				action = test["action"]
			except KeyError:
				pass
			action_str = str(action) if action else None

			ok = False
			try:
				if action_str == "parse_fail_msg":
					ok = test_parse_fail_msg(src, test)
				elif action_str == "parse_fail_code":
					ok = test_parse_fail_code(src, test)
				elif action_str == "roundtrip":
					ok = test_roundtrip(src, test)
				elif action_str is None:
					ok = test_check_all(src, test)
			except Exception:
				ok = False

			if ok:
				passed += 1
				print(f"  {_ansi_green}✓{_ansi_reset}  {name}")
			else:
				failed += 1
				print(f"  {_ansi_red}✗{_ansi_reset}  {name}")

		print("\n══════════════════════════════")
		print(f"  {_ansi_green}Passed: {passed}{_ansi_reset}")
		print(f"  {'Failed: ' + str(failed) if failed == 0 else _ansi_red + 'Failed: ' + str(failed) + _ansi_reset}")
		print("══════════════════════════════")
		if failed > 0:
			raise SystemExit(1)
