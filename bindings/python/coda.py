"""
Python FFI bindings for Coda configuration format parser.

This module provides a Pythonic interface to the Coda C library using ctypes.
"""

import ctypes
import ctypes.util
from ctypes import c_char_p, c_size_t, c_uint32, c_void_p, POINTER, Structure
from typing import Iterator, Optional, Tuple
import os
import sys


# ------------------------- Find and load the library -------------------------

def _find_library():
	"""Locate the coda FFI shared library."""
	# Try common library names and locations
	lib_names = []
	
	if sys.platform == "win32":
		lib_names = ["coda_ffi.dll", "libcoda_ffi.dll"]
	elif sys.platform == "darwin":
		lib_names = ["libcoda_ffi.dylib", "coda_ffi.dylib"]
	else:  # Linux and other Unix-like
		lib_names = ["libcoda_ffi.so", "coda_ffi.so"]
	
	# Determine architecture-specific dist subdirectory
	import platform
	machine = platform.machine().lower()
	
	# Map platform.machine() to dist directory names
	arch_map = {
		'x86_64': 'x86_64',
		'amd64': 'x86_64',
		'aarch64': 'aarch64',
		'arm64': 'aarch64',
	}
	arch = arch_map.get(machine, machine)
	
	# Determine libc type (gnu vs musl) - default to gnu
	libc = 'gnu'
	try:
		with open('/usr/bin/ldd', 'rb') as f:
			if b'musl' in f.read():
				libc = 'musl'
	except:
		pass
	
	dist_subdir = f"{arch}-linux-{libc}"
	if sys.platform == "win32":
		dist_subdir = f"{arch}-windows-gnu"
	elif sys.platform == "darwin":
		dist_subdir = f"{arch}-macos"
	
	# Prefer system-installed library when available
	for lib_name in lib_names:
		found = ctypes.util.find_library(lib_name)
		if found:
			return found

	# Search paths
	script_dir = os.path.dirname(os.path.abspath(__file__))
	repo_root = os.path.join(script_dir, "..", "..")
	
	search_paths = [
		os.path.join(repo_root, "dist", dist_subdir),  # Architecture-specific dist
		os.path.join(repo_root, "dist", f"x86_64-linux-gnu"),  # Fallback to x86_64-gnu
		os.path.join(repo_root, "dist"),
		script_dir,  # Same directory as this file
		os.path.join(repo_root, "build"),
		".",
	]
	
	for path in search_paths:
		for lib_name in lib_names:
			lib_path = os.path.join(path, lib_name)
			if os.path.exists(lib_path):
				return lib_path
	
	# Fall back to letting ctypes find it in system paths
	return lib_names[0]


_lib_path = _find_library()
try:
	_lib = ctypes.CDLL(_lib_path)
except OSError as e:
	raise ImportError(
		f"Failed to load Coda FFI library from {_lib_path}. "
		"Make sure the library is built and in the correct location."
	) from e


# ------------------------- C Types and Structures -------------------------

class CodaStr(Structure):
	"""Borrowed string view (non-owning)."""
	_fields_ = [
		("ptr", c_char_p),
		("len", c_size_t),
	]
	
	def to_python(self) -> str:
		"""Convert to Python string."""
		if self.ptr is None:
			return ""
		return self.ptr[:self.len].decode('utf-8', errors='replace')


class CodaOwnedStr(Structure):
	"""Owned string (must be freed)."""
	_fields_ = [
		("ptr", c_char_p),
		("len", c_size_t),
	]
	
	def to_python(self) -> str:
		"""Convert to Python string."""
		if self.ptr is None:
			return ""
		return self.ptr[:self.len].decode('utf-8', errors='replace')


class CodaError(Structure):
	"""Error information from parser."""
	_fields_ = [
		("code", c_uint32),
		("line", c_uint32),
		("col", c_uint32),
		("offset", c_size_t),
		("message", CodaOwnedStr),
	]


# Node types
CODA_NODE_NULL = 0
CODA_NODE_FILE = 1
CODA_NODE_STRING = 2
CODA_NODE_BLOCK = 3
CODA_NODE_ARRAY = 4
CODA_NODE_TABLE = 5

# Status codes
CODA_OK = 0
CODA_ERR = 1
CODA_NOT_FOUND = 2
CODA_BAD_KIND = 3
CODA_OUT_OF_RANGE = 4


# ------------------------- Function Signatures -------------------------

# Memory management
_lib.coda_free.argtypes = [c_void_p]
_lib.coda_free.restype = None

_lib.coda_error_clear.argtypes = [POINTER(CodaError)]
_lib.coda_error_clear.restype = None

_lib.coda_owned_str_free.argtypes = [CodaOwnedStr]
_lib.coda_owned_str_free.restype = None

# ABI version
_lib.coda_ffi_abi_version.argtypes = []
_lib.coda_ffi_abi_version.restype = c_uint32

# Doc lifecycle
_lib.coda_doc_new.argtypes = []
_lib.coda_doc_new.restype = c_void_p

_lib.coda_doc_free.argtypes = [c_void_p]
_lib.coda_doc_free.restype = None

_lib.coda_doc_parse.argtypes = [c_char_p, c_size_t, c_char_p, POINTER(CodaError)]
_lib.coda_doc_parse.restype = c_void_p

_lib.coda_doc_parse_file.argtypes = [c_char_p, POINTER(CodaError)]
_lib.coda_doc_parse_file.restype = c_void_p

_lib.coda_doc_serialize.argtypes = [c_void_p, c_char_p, c_size_t, POINTER(CodaError)]
_lib.coda_doc_serialize.restype = CodaOwnedStr

_lib.coda_doc_order.argtypes = [c_void_p]
_lib.coda_doc_order.restype = None

_lib.coda_doc_order_weighted.argtypes = [c_void_p, POINTER(c_char_p), POINTER(ctypes.c_float), c_size_t]
_lib.coda_doc_order_weighted.restype = None

_lib.coda_doc_root.argtypes = [c_void_p]
_lib.coda_doc_root.restype = c_uint32

# Node inspection
_lib.coda_node_kind.argtypes = [c_void_p, c_uint32]
_lib.coda_node_kind.restype = c_uint32

_lib.coda_node_comment.argtypes = [c_void_p, c_uint32]
_lib.coda_node_comment.restype = CodaStr

_lib.coda_node_set_comment.argtypes = [c_void_p, c_uint32, c_char_p, c_size_t]
_lib.coda_node_set_comment.restype = c_uint32

# String nodes
_lib.coda_string_get.argtypes = [c_void_p, c_uint32]
_lib.coda_string_get.restype = CodaStr

_lib.coda_string_set.argtypes = [c_void_p, c_uint32, c_char_p, c_size_t]
_lib.coda_string_set.restype = c_uint32

# Array nodes
_lib.coda_array_len.argtypes = [c_void_p, c_uint32]
_lib.coda_array_len.restype = c_size_t

_lib.coda_array_get.argtypes = [c_void_p, c_uint32, c_size_t]
_lib.coda_array_get.restype = c_uint32

_lib.coda_array_set.argtypes = [c_void_p, c_uint32, c_size_t, c_uint32]
_lib.coda_array_set.restype = c_uint32

_lib.coda_array_push.argtypes = [c_void_p, c_uint32, c_uint32]
_lib.coda_array_push.restype = c_uint32

# Map-like nodes
_lib.coda_map_len.argtypes = [c_void_p, c_uint32]
_lib.coda_map_len.restype = c_size_t

_lib.coda_map_key_at.argtypes = [c_void_p, c_uint32, c_size_t]
_lib.coda_map_key_at.restype = CodaStr

_lib.coda_map_value_at.argtypes = [c_void_p, c_uint32, c_size_t]
_lib.coda_map_value_at.restype = c_uint32

_lib.coda_map_get.argtypes = [c_void_p, c_uint32, c_char_p, c_size_t]
_lib.coda_map_get.restype = c_uint32

_lib.coda_map_get_or_insert.argtypes = [c_void_p, c_uint32, c_char_p, c_size_t]
_lib.coda_map_get_or_insert.restype = c_uint32

_lib.coda_map_set.argtypes = [c_void_p, c_uint32, c_char_p, c_size_t, c_uint32]
_lib.coda_map_set.restype = c_uint32

_lib.coda_map_remove.argtypes = [c_void_p, c_uint32, c_char_p, c_size_t]
_lib.coda_map_remove.restype = c_uint32

# Node creation
_lib.coda_new_string.argtypes = [c_void_p, c_char_p, c_size_t]
_lib.coda_new_string.restype = c_uint32

_lib.coda_new_block.argtypes = [c_void_p]
_lib.coda_new_block.restype = c_uint32

_lib.coda_new_array.argtypes = [c_void_p]
_lib.coda_new_array.restype = c_uint32

_lib.coda_new_table.argtypes = [c_void_p]
_lib.coda_new_table.restype = c_uint32


# ------------------------- Exceptions -------------------------

class CodaException(Exception):
	"""Base exception for Coda errors."""
	pass


class CodaParseError(CodaException):
	"""Parsing error with location information."""
	
	def __init__(self, message: str, code: int = 0, line: int = 0, col: int = 0, offset: int = 0):
		super().__init__(message)
		self.code = code
		self.line = line
		self.col = col
		self.offset = offset
	
	def __str__(self):
		if self.line > 0 and self.col > 0:
			return f"{super().__str__()} (line {self.line}, col {self.col})"
		return super().__str__()


# ------------------------- Python Interface -------------------------

class Coda:
	"""
	Represents a single value or node in a Coda document.
	
	This is a lightweight wrapper around a node ID that provides
	a Pythonic interface to access and manipulate Coda data.
	"""
	
	def __init__(self, doc: 'CodaDoc', node_id: int):
		"""
		Initialize a Coda node wrapper.
		
		Args:
			doc: The parent CodaDoc
			node_id: The node ID in the document
		"""
		self._doc = doc
		self._node_id = node_id
	
	def _check_valid(self):
		"""Ensure the document is still valid."""
		if self._doc._ptr is None:
			raise CodaException("CodaDoc has been freed")
	
	@property
	def kind(self) -> int:
		"""Get the kind of this node."""
		self._check_valid()
		return _lib.coda_node_kind(self._doc._ptr, self._node_id)
	
	def asString(self) -> str:
		"""Get this node as a string value."""
		self._check_valid()
		result = _lib.coda_string_get(self._doc._ptr, self._node_id)
		return result.to_python()
	
	def asArray(self) -> Iterator['Coda']:
		"""Iterate over array elements."""
		self._check_valid()
		if self.kind != CODA_NODE_ARRAY:
			raise TypeError("node is not an array")
		length = _lib.coda_array_len(self._doc._ptr, self._node_id)
		for i in range(length):
			child_id = _lib.coda_array_get(self._doc._ptr, self._node_id, i)
			if child_id != 0:
				yield Coda(self._doc, child_id)
	
	def asBlock(self) -> Iterator[Tuple[str, 'Coda']]:
		"""Iterate over block/map key-value pairs."""
		self._check_valid()
		if self.kind not in (CODA_NODE_FILE, CODA_NODE_BLOCK, CODA_NODE_TABLE):
			raise TypeError("node is not a map")
		length = _lib.coda_map_len(self._doc._ptr, self._node_id)
		for i in range(length):
			key = _lib.coda_map_key_at(self._doc._ptr, self._node_id, i)
			value_id = _lib.coda_map_value_at(self._doc._ptr, self._node_id, i)
			if value_id != 0:
				yield (key.to_python(), Coda(self._doc, value_id))

	def asTable(self) -> Iterator[Tuple[str, 'Coda']]:
		"""Iterate over keyed table rows."""
		self._check_valid()
		if self.kind != CODA_NODE_TABLE:
			raise TypeError("node is not a table")
		return self.asBlock()
	
	def __getitem__(self, key: str) -> 'Coda':
		"""Access a child node by key (for blocks/maps)."""
		self._check_valid()
		key_bytes = key.encode('utf-8')
		child_id = _lib.coda_map_get(self._doc._ptr, self._node_id, key_bytes, len(key_bytes))
		if child_id == 0:
			raise KeyError(f"Key not found: {key}")
		return Coda(self._doc, child_id)
	
	def __setitem__(self, key: str, value: str):
		"""Set a child node value (for blocks/maps)."""
		self._check_valid()
		# Create a new string node
		value_bytes = value.encode('utf-8')
		value_id = _lib.coda_new_string(self._doc._ptr, value_bytes, len(value_bytes))
		if value_id == 0:
			raise CodaException("Failed to create string node")
		
		# Set it in the map
		key_bytes = key.encode('utf-8')
		status = _lib.coda_map_set(self._doc._ptr, self._node_id, key_bytes, len(key_bytes), value_id)
		if status != CODA_OK:
			raise CodaException(f"Failed to set key: {key}")
	
	def get(self, key: str, default: Optional[str] = None) -> Optional[str]:
		"""Get a value by key with optional default."""
		try:
			return self[key].asString()
		except KeyError:
			return default

	def get_or_insert(self, key: str) -> 'Coda':
		"""Get or insert a child node by key (for blocks/maps)."""
		self._check_valid()
		key_bytes = key.encode('utf-8')
		child_id = _lib.coda_map_get_or_insert(
			self._doc._ptr, self._node_id, key_bytes, len(key_bytes)
		)
		if child_id == 0:
			raise CodaException(f"Failed to insert key: {key}")
		return Coda(self._doc, child_id)

	@property
	def comment(self) -> str:
		"""Get the comment attached to this node."""
		self._check_valid()
		result = _lib.coda_node_comment(self._doc._ptr, self._node_id)
		return result.to_python()

	@comment.setter
	def comment(self, value: str):
		"""Set the comment for this node."""
		self._check_valid()
		value_bytes = value.encode('utf-8')
		status = _lib.coda_node_set_comment(self._doc._ptr, self._node_id, value_bytes, len(value_bytes))
		if status != CODA_OK:
			raise CodaException("Failed to set comment")


class CodaTestRunner:
	"""Executes catalog-driven tests against a CodaDoc."""

	def __init__(self, doc: CodaDoc):
		self.doc = doc

	def _parse_bool(self, value: str) -> bool:
		return value in ("true", "1", "yes")

	def _parse_int(self, value: str) -> int:
		return int(value)

	def _parse_float(self, value: str) -> float:
		return float(value)

	def _array_as_strings(self, arr: Coda) -> list[str]:
		return [v.asString() for v in arr.asArray()]

	def _check_order_contains(self, text: str, order: list[str]) -> bool:
		pos = 0
		for needle in order:
			found = text.find(needle, pos)
			if found < 0:
				return False
			pos = found + len(needle)
		return True

	def run_check(self, check: Coda) -> bool:
		op = check["op"].asString()

		if op == "get_string":
			return self.doc[check["field"].asString()].asString() == check["eq"].asString()

		if op == "get_string_path":
			path = self._array_as_strings(check["path"])
			node = self.doc[path[0]]
			for key in path[1:]:
				node = node[key]
			return node.asString() == check["eq"].asString()

		if op == "is_container":
			node = self.doc[check["field"].asString()]
			return (node.kind in (CODA_NODE_BLOCK, CODA_NODE_ARRAY, CODA_NODE_TABLE)) == self._parse_bool(check["eq_bool"].asString())

		if op == "has_key":
			key = check["field"].asString()
			try:
				_ = self.doc[key]
				got = True
			except KeyError:
				got = False
			return got == self._parse_bool(check["eq_bool"].asString())

		if op == "map_len":
			node = self.doc[check["field"].asString()]
			return len(list(node.asBlock())) == self._parse_int(check["eq_int"].asString())

		if op == "map_keys":
			node = self.doc[check["field"].asString()]
			keys = [k for k, _ in node.asBlock()]
			return keys == self._array_as_strings(check["eq_list"])

		if op == "array_len":
			node = self.doc[check["field"].asString()]
			return len(list(node.asArray())) == self._parse_int(check["eq_int"].asString())

		if op == "array_element":
			node = self.doc[check["field"].asString()]
			idx = self._parse_int(check["idx"].asString())
			return list(node.asArray())[idx].asString() == check["eq"].asString()

		if op == "array_block_count":
			node = self.doc[check["field"].asString()]
			return len(list(node.asArray())) == self._parse_int(check["eq_int"].asString())

		if op == "array_block_field":
			node = self.doc[check["field"].asString()]
			idx = self._parse_int(check["idx"].asString())
			field = check["field_name"].asString()
			return list(node.asArray())[idx][field].asString() == check["eq"].asString()

		if op == "array_index_throws":
			node = self.doc[check["field"].asString()]
			idx = self._parse_int(check["idx"].asString())
			try:
				_ = list(node.asArray())[idx]
				got = False
			except Exception:
				got = True
			return got == self._parse_bool(check["eq_bool"].asString())

		if op == "plain_table_cell":
			table = self.doc[check["table"].asString()]
			idx = self._parse_int(check["idx"].asString())
			col = check["col"].asString()
			return list(table.asArray())[idx][col].asString() == check["eq"].asString()

		if op == "table_cell":
			table = self.doc[check["table"].asString()]
			row = check["row"].asString()
			col = check["col"].asString()
			return table[row][col].asString() == check["eq"].asString()

		if op == "table_row_keys":
			table = self.doc[check["table"].asString()]
			keys = [k for k, _ in table.asTable()]
			return keys == self._array_as_strings(check["eq_list"])

		if op == "table_row_missing_inserts":
			table = self.doc[check["table"].asString()]
			row = check["row"].asString()
			try:
				node = table.get_or_insert(row)
				got = node.asString() == ""
			except Exception:
				got = False
			return got == self._parse_bool(check["eq_bool"].asString())

		if op == "table_row_missing_throws":
			table = self.doc[check["table"].asString()]
			row = check["row"].asString()
			try:
				_ = table[row]
				got = False
			except KeyError:
				got = True
			return got == self._parse_bool(check["eq_bool"].asString())

		if op == "comment":
			node = self.doc[check["field"].asString()]
			return node.comment == check["eq"].asString()

		if op == "comment_path":
			path = self._array_as_strings(check["path"])
			node = self.doc[path[0]]
			for key in path[1:]:
				node = node[key]
			return node.comment == check["eq"].asString()

		if op == "array_element_comment":
			node = self.doc[check["field"].asString()]
			idx = self._parse_int(check["idx"].asString())
			return list(node.asArray())[idx].comment == check["eq"].asString()

		if op == "table_row_comment":
			table = self.doc[check["table"].asString()]
			row = check["row"].asString()
			return table[row].comment == check["eq"].asString()

		if op == "plain_table_row_comment":
			table = self.doc[check["table"].asString()]
			idx = self._parse_int(check["idx"].asString())
			return list(table.asArray())[idx].comment == check["eq"].asString()

		if op == "set_string":
			self.doc[check["field"].asString()] = check["value"].asString()
			return self.doc[check["field"].asString()].asString() == check["value"].asString()

		if op == "set_string_path":
			path = self._array_as_strings(check["path"])
			node = self.doc[path[0]]
			for key in path[1:-1]:
				node = node[key]
			node[path[-1]] = check["value"].asString()
			return node[path[-1]].asString() == check["value"].asString()

		if op == "string_index_on_scalar_throws":
			try:
				_ = self.doc[check["field"].asString()][check["sub"].asString()]
				got = False
			except Exception:
				got = True
			return got == self._parse_bool(check["eq_bool"].asString())

		if op == "int_index_on_block_throws":
			try:
				_ = list(self.doc[check["field"].asString()].asArray())[self._parse_int(check["idx"].asString())]
				got = False
			except Exception:
				got = True
			return got == self._parse_bool(check["eq_bool"].asString())

		if op == "as_array_on_scalar_throws":
			try:
				_ = list(self.doc[check["field"].asString()].asArray())
				got = False
			except Exception:
				got = True
			return got == self._parse_bool(check["eq_bool"].asString())

		if op == "as_block_on_array_throws":
			try:
				_ = list(self.doc[check["field"].asString()].asBlock())
				got = False
			except Exception:
				got = True
			return got == self._parse_bool(check["eq_bool"].asString())

		if op == "as_table_on_block_throws":
			try:
				_ = list(self.doc[check["field"].asString()].asTable())
				got = False
			except Exception:
				got = True
			return got == self._parse_bool(check["eq_bool"].asString())

		if op == "const_missing_key_throws":
			try:
				_ = self.doc[check["field"].asString()]
				got = False
			except KeyError:
				got = True
			return got == self._parse_bool(check["eq_bool"].asString())

		if op == "order_default_contains_order":
			order = self._array_as_strings(check["order"])
			self.doc.order()
			return self._check_order_contains(self.doc.serialize(), order)

		if op == "order_weighted_contains_order":
			order = self._array_as_strings(check["order"])
			weights = []
			for entry in check["weights"].asArray():
				weights.append((entry["field"].asString(), self._parse_float(entry["weight"].asString())))
			return self._check_order_contains(self.doc.order_weighted_and_serialize(weights), order)

		if op == "serialize_contains":
			indent = check["indent"].asString()
			text = self.doc.serialize(indent)
			return check["contains"].asString() in text

		return False


def run_catalog_tests(catalog_path: str) -> None:
	ansi_green = "\033[32m"
	ansi_red = "\033[31m"
	ansi_yellow = "\033[33m"
	ansi_reset = "\033[0m"

	with open(catalog_path, "r", encoding="utf-8") as f:
		catalog_text = f.read()

	catalog = CodaDoc.parse(catalog_text)
	try:
		tests = list(catalog["tests"].asArray())

		passed = 0
		failed = 0
		current_suite = None

		for test in tests:
			suite = test["suite"].asString()
			name = test["name"].asString()
			src = test["src"].asString()

			if suite != current_suite:
				current_suite = suite
				print(f"\n{ansi_yellow}[{suite}]{ansi_reset}")

			action = None
			try:
				action = test["action"].asString()
			except KeyError:
				action = None

			ok = False
			try:
				if action:
					if action == "parse_fail_msg":
						try:
							CodaDoc.parse(src)
							ok = False
						except CodaParseError as e:
							needles = [v.asString() for v in test["needles"].asArray()]
							ok = any(n in str(e) for n in needles) or not needles
					elif action == "parse_fail_code":
						try:
							CodaDoc.parse(src)
							ok = False
						except CodaParseError as e:
							code_map = {
								"UnexpectedToken": 0,
								"UnexpectedEOF": 1,
								"DuplicateKey": 2,
								"DuplicateField": 3,
								"RaggedRow": 4,
								"CommentBeforeHeader": 5,
								"InvalidEscape": 6,
								"UnterminatedString": 7,
								"NestedBlock": 8,
								"ContentAfterBrace": 9,
								"KeyInBlock": 10,
							}
							expected = code_map.get(test["code"].asString(), -1)
							ok = int(e.code) == expected
					elif action == "roundtrip":
						with CodaDoc.parse(src) as d1:
							s1 = d1.serialize()
						with CodaDoc.parse(s1) as d2:
							s2 = d2.serialize()
						ok = s1 == s2
					else:
						ok = False
				else:
					with CodaDoc.parse(src) as doc:
						runner = CodaTestRunner(doc)
						ok = True
						try:
							checks = list(test["checks"].asArray())
						except KeyError:
							checks = []
						for check in checks:
							if not runner.run_check(check):
								ok = False
								break
			except Exception:
				ok = False

			if ok:
				passed += 1
				print(f"  {ansi_green}✓{ansi_reset}  {name}")
			else:
				failed += 1
				print(f"  {ansi_red}✗{ansi_reset}  {name}")
				print("	  returned false")

		print("\n══════════════════════════════")
		print(f"  {ansi_green}Passed: {passed}{ansi_reset}")
		if failed > 0:
			print(f"  {ansi_red}Failed: {failed}{ansi_reset}")
		else:
			print(f"  Failed: {failed}")
		print("══════════════════════════════")
		if failed > 0:
			raise SystemExit(1)
	finally:
		catalog.free()


class CodaDoc:
	"""
	Represents a parsed Coda document.
	
	Use as a context manager to ensure proper cleanup:
		with CodaDoc.parse(text) as doc:
			print(doc["name"].asString())
	"""
	
	def __init__(self, ptr: c_void_p):
		"""
		Initialize with a raw pointer (internal use).
		
		Use CodaDoc.parse() or CodaDoc.parse_file() instead.
		"""
		self._ptr = ptr
	
	@classmethod
	def parse(cls, text: str, filename: Optional[str] = None) -> 'CodaDoc':
		"""
		Parse a Coda document from a string.
		
		Args:
			text: The Coda text to parse
			filename: Optional filename for error messages
		
		Returns:
			A new CodaDoc instance
		
		Raises:
			CodaParseError: If parsing fails
		"""
		text_bytes = text.encode('utf-8')
		filename_bytes = filename.encode('utf-8') if filename else None
		
		err = CodaError()
		ptr = _lib.coda_doc_parse(
			text_bytes,
			len(text_bytes),
			filename_bytes,
			ctypes.byref(err)
		)
		
		if ptr is None:
			message = err.message.to_python()
			code = err.code
			line = err.line
			col = err.col
			offset = err.offset
			_lib.coda_error_clear(ctypes.byref(err))
			raise CodaParseError(
				message,
				code=code,
				line=line,
				col=col,
				offset=offset
			)
		
		return cls(ptr)
	
	@classmethod
	def parse_file(cls, path: str) -> 'CodaDoc':
		"""
		Parse a Coda document from a file.
		
		Args:
			path: Path to the Coda file
		
		Returns:
			A new CodaDoc instance
		
		Raises:
			CodaParseError: If parsing fails or file cannot be read
		"""
		path_bytes = path.encode('utf-8')
		
		err = CodaError()
		ptr = _lib.coda_doc_parse_file(path_bytes, ctypes.byref(err))
		
		if ptr is None:
			message = err.message.to_python()
			code = err.code
			line = err.line
			col = err.col
			offset = err.offset
			_lib.coda_error_clear(ctypes.byref(err))
			raise CodaParseError(
				message,
				code=code,
				line=line,
				col=col,
				offset=offset
			)
		
		return cls(ptr)
	
	@classmethod
	def new(cls) -> 'CodaDoc':
		"""Create a new empty Coda document."""
		ptr = _lib.coda_doc_new()
		if ptr is None:
			raise CodaException("Failed to create new document")
		return cls(ptr)
	
    def __enter__(self) -> "CodaDoc":
        """Context manager entry."""
        return self
	
	def __exit__(self, exc_type, exc_val, exc_tb):
		"""Context manager exit - ensures cleanup."""
		self.free()
		return False
	
	def free(self):
		"""Free the document resources."""
		if self._ptr is not None:
			_lib.coda_doc_free(self._ptr)
			self._ptr = None
	
	def __del__(self):
		"""Destructor - free resources if not already freed."""
		self.free()
	
	def root(self) -> Coda:
		"""Get the root node of the document."""
		if self._ptr is None:
			raise CodaException("CodaDoc has been freed")
		root_id = _lib.coda_doc_root(self._ptr)
		return Coda(self, root_id)
	
	def __getitem__(self, key: str) -> Coda:
		"""Access a top-level key directly."""
		return self.root().__getitem__(key)
	
	def __setitem__(self, key: str, value: str):
		"""Set a top-level key directly."""
		self.root().__setitem__(key, value)
	
	def serialize(self, indent: str = "\t") -> str:
		"""
		Serialize the document to Coda format.
		
		Args:
			indent: Indentation unit (default: tab)
		
		Returns:
			The serialized Coda text
		
		Raises:
			CodaException: If serialization fails
		"""
		if self._ptr is None:
			raise CodaException("CodaDoc has been freed")
		
		indent_bytes = indent.encode('utf-8')
		err = CodaError()
		
		result = _lib.coda_doc_serialize(
			self._ptr,
			indent_bytes,
			len(indent_bytes),
			ctypes.byref(err)
		)
		
		if result.ptr is None:
			message = err.message.to_python()
			_lib.coda_error_clear(ctypes.byref(err))
			raise CodaException(f"Serialization failed: {message}")
		
		# Convert to Python string and free the C string
		text = result.to_python()
		_lib.coda_owned_str_free(result)
		
		return text

	def order(self) -> None:
		"""Reorder fields using default ordering."""
		if self._ptr is None:
			raise CodaException("CodaDoc has been freed")
		_lib.coda_doc_order(self._ptr)

	def order_weighted_and_serialize(self, weights: list[tuple[str, float]]) -> str:
		"""Order fields with weights and serialize with tabs."""
		if self._ptr is None:
			raise CodaException("CodaDoc has been freed")
		if weights:
			keys = (c_char_p * len(weights))()
			vals = (ctypes.c_float * len(weights))()
			for i, (k, v) in enumerate(weights):
				keys[i] = k.encode('utf-8')
				vals[i] = v
			_lib.coda_doc_order_weighted(self._ptr, keys, vals, len(weights))
		else:
			_lib.coda_doc_order(self._ptr)
		return self.serialize("\t")
	
	def save(self, path: str, indent: str = "\t"):
		"""
		Save the document to a file.
		
		Args:
			path: Path to save the file
			indent: Indentation unit (default: tab)
		"""
		text = self.serialize(indent)
		with open(path, 'w', encoding='utf-8') as f:
			f.write(text)


# ------------------------- Module-level utilities -------------------------

def get_abi_version() -> int:
	"""Get the ABI version of the loaded library."""
	return _lib.coda_ffi_abi_version()


__all__ = [
	'Coda',
	'CodaDoc',
	'CodaException',
	'CodaParseError',
	'get_abi_version',
]
