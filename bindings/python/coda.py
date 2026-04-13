"""
Python FFI bindings for the Coda configuration format.

One class per AST node type, mirroring the C++ architecture:

    CodaBlock       ← { key value ... } — also used as the root node
    CodaArray       ← [ ... ]              homogeneous or nested values
    CodaTable       ← [ col1 col2 \n ... ] anonymous-row plain table
    CodaKeyedTable  ← [ key col1 col2 \n rowkey val1 val2 ] keyed table
    CodaRow         ← one row inside a Table or KeyedTable
    CodaString      ← a leaf string value

Usage mirrors C++:

    with CodaDoc.parse(text) as doc:
        root = doc.root()
        name = root["name"]                     # CodaString
        for key, val in root["block"]:          # CodaBlock iteration
            ...
        for row in root["table"]:               # CodaTable → CodaRow
            print(row["col"])
        for key, row in root["ktable"]:         # CodaKeyedTable → (str, CodaRow)
            print(key, row["col"])
"""

import ctypes
import ctypes.util
from ctypes import c_char_p, c_int, c_size_t, c_uint32, c_void_p, POINTER, Structure
import os
import sys
from typing import Iterator, Optional, Self, Tuple, Union


# ─── Find and load the library ───────────────────────────────────────────────

def _find_library() -> str:
	env_path = os.environ.get("CODA_FFI_LIB")
	if env_path:
		return env_path

	if sys.platform == "win32":
		lib_names = ["libcoda_ffi.dll", "coda_ffi.dll"]
	elif sys.platform == "darwin":
		lib_names = ["libcoda_ffi.dylib", "coda_ffi.dylib"]
	else:
		lib_names = ["libcoda_ffi.so", "coda_ffi.so"]

	import platform
	machine = platform.machine().lower()
	arch = {"x86_64": "x86_64", "amd64": "x86_64",
	        "aarch64": "aarch64", "arm64": "aarch64"}.get(machine, machine)

	libc = "gnu"
	try:
		with open("/usr/bin/ldd", "rb") as f:
			if b"musl" in f.read():
				libc = "musl"
	except Exception:
		pass

	if sys.platform == "win32":
		dist_subdir = f"{arch}-windows-gnu"
	elif sys.platform == "darwin":
		dist_subdir = f"{arch}-macos"
	else:
		dist_subdir = f"{arch}-linux-{libc}"

	# Check repo-local paths first (dist/, build/), then fall back to the
	# bare library name so the OS linker resolves it via LD_LIBRARY_PATH /
	# ldconfig — same mechanism as rake install.
	script_dir = os.path.dirname(os.path.abspath(__file__))
	repo_root  = os.path.join(script_dir, "..", "..")
	for path in [
		os.path.join(repo_root, "dist", dist_subdir),
		os.path.join(repo_root, "dist", "x86_64-linux-gnu"),
		os.path.join(repo_root, "dist"),
		script_dir,
		os.path.join(repo_root, "build"),
	]:
		for lib_name in lib_names:
			lib_path = os.path.join(path, lib_name)
			if os.path.exists(lib_path):
				return lib_path

	# Let the OS find it (installed via rake install / ldconfig).
	return lib_names[0]


_lib_path = _find_library()
try:
	_lib = ctypes.CDLL(_lib_path)
except OSError as e:
	raise ImportError(
		f"Failed to load Coda FFI library from {_lib_path}. "
		"Make sure the library is built and in the correct location."
	) from e

_lib.coda_ffi_abi_version.restype  = c_uint32
_lib.coda_ffi_abi_version.argtypes = []
_abi = _lib.coda_ffi_abi_version()
if _abi != 3:
	raise ImportError(
		f"Incompatible libcoda_ffi ABI: expected 3, got {_abi} (library: {_lib_path})"
	)


# ─── C structures ─────────────────────────────────────────────────────────────

class _CodaStr(Structure):
	_fields_ = [("ptr", c_char_p), ("len", c_size_t)]
	def to_python(self) -> str:
		return self.ptr[:self.len].decode("utf-8", errors="replace") if self.ptr else ""

class _CodaOwnedStr(Structure):
	_fields_ = [("ptr", c_char_p), ("len", c_size_t)]
	def to_python(self) -> str:
		return self.ptr[:self.len].decode("utf-8", errors="replace") if self.ptr else ""
	def to_python_and_free(self) -> str:
		"""Decode to Python str and free the underlying C buffer."""
		result = self.to_python()
		if self.ptr:
			_lib.coda_owned_str_free(self)
		return result

class _CodaError(Structure):
	_fields_ = [
		("code",    c_uint32),
		("line",    c_uint32),
		("col",     c_uint32),
		("offset",  c_size_t),
		("message", _CodaOwnedStr),
	]


# ─── Node kind constants (internal) ───────────────────────────────────────────

_NODE_NULL        = 0
_NODE_STRING      = 2
_NODE_BLOCK       = 3
_NODE_ARRAY       = 4
_NODE_TABLE       = 5
_NODE_KEYED_TABLE = 6
_NODE_ROW         = 7

_CODA_OK           = 0
_CODA_NOT_FOUND    = 2
_CODA_BAD_KIND     = 3
_CODA_OUT_OF_RANGE = 4


# ─── Function signatures ──────────────────────────────────────────────────────

def _sig(fn, restype, *argtypes):
	fn.restype  = restype
	fn.argtypes = list(argtypes)

_sig(_lib.coda_free,            None,          c_void_p)
_sig(_lib.coda_error_clear,     None,          POINTER(_CodaError))
_sig(_lib.coda_owned_str_free,  None,          _CodaOwnedStr)
_sig(_lib.coda_ffi_abi_version, c_uint32)

_sig(_lib.coda_doc_new,            c_void_p)
_sig(_lib.coda_doc_free,           None,          c_void_p)
_sig(_lib.coda_doc_parse,          c_void_p,      c_char_p, c_size_t, c_char_p, POINTER(_CodaError))
_sig(_lib.coda_doc_parse_file,     c_void_p,      c_char_p, POINTER(_CodaError))
_sig(_lib.coda_doc_serialize,      _CodaOwnedStr, c_void_p, c_char_p, c_size_t, POINTER(_CodaError))
_sig(_lib.coda_doc_order,          None,          c_void_p)
_sig(_lib.coda_doc_order_weighted, None,          c_void_p, POINTER(c_char_p), POINTER(ctypes.c_float), c_size_t)
_sig(_lib.coda_doc_root,           c_uint32,      c_void_p)

_sig(_lib.coda_node_kind,               c_uint32, c_void_p, c_uint32)
_sig(_lib.coda_node_is_container,       c_int,    c_void_p, c_uint32)
_sig(_lib.coda_node_comment_get,        _CodaStr, c_void_p, c_uint32)
_sig(_lib.coda_node_comment_set,        c_uint32, c_void_p, c_uint32, c_char_p, c_size_t)
_sig(_lib.coda_node_header_comment_get, _CodaStr, c_void_p, c_uint32)
_sig(_lib.coda_node_header_comment_set, c_uint32, c_void_p, c_uint32, c_char_p, c_size_t)
_sig(_lib.coda_node_serialize,          _CodaOwnedStr, c_void_p, c_uint32, c_char_p, c_size_t, POINTER(_CodaError))
_sig(_lib.coda_node_order,              None,     c_void_p, c_uint32)
_sig(_lib.coda_node_order_weighted,     None,     c_void_p, c_uint32, POINTER(c_char_p), POINTER(ctypes.c_float), c_size_t)

_sig(_lib.coda_string_get, _CodaStr, c_void_p, c_uint32)
_sig(_lib.coda_string_set, c_uint32, c_void_p, c_uint32, c_char_p, c_size_t)

_sig(_lib.coda_array_len,    c_size_t, c_void_p, c_uint32)
_sig(_lib.coda_array_get,    c_uint32, c_void_p, c_uint32, c_size_t)
_sig(_lib.coda_array_set,    c_uint32, c_void_p, c_uint32, c_size_t, c_uint32)
_sig(_lib.coda_array_push,   c_uint32, c_void_p, c_uint32, c_uint32)
_sig(_lib.coda_array_remove, c_uint32, c_void_p, c_uint32, c_size_t)

_sig(_lib.coda_map_len,           c_size_t, c_void_p, c_uint32)
_sig(_lib.coda_map_key_at,        _CodaStr, c_void_p, c_uint32, c_size_t)
_sig(_lib.coda_map_value_at,      c_uint32, c_void_p, c_uint32, c_size_t)
_sig(_lib.coda_map_get,           c_uint32, c_void_p, c_uint32, c_char_p, c_size_t)
_sig(_lib.coda_map_get_or_insert, c_uint32, c_void_p, c_uint32, c_char_p, c_size_t)
_sig(_lib.coda_map_set,           c_uint32, c_void_p, c_uint32, c_char_p, c_size_t, c_uint32)
_sig(_lib.coda_map_remove,        c_uint32, c_void_p, c_uint32, c_char_p, c_size_t)

_sig(_lib.coda_table_col_count,  c_size_t, c_void_p, c_uint32)
_sig(_lib.coda_table_col_name,   _CodaStr, c_void_p, c_uint32, c_size_t)
_sig(_lib.coda_table_col_append, c_uint32, c_void_p, c_uint32, c_char_p, c_size_t)
_sig(_lib.coda_table_row_count,  c_size_t, c_void_p, c_uint32)
_sig(_lib.coda_table_row_at,     c_uint32, c_void_p, c_uint32, c_size_t)
_sig(_lib.coda_table_row_append, c_uint32, c_void_p, c_uint32, c_uint32)
_sig(_lib.coda_table_row_set,    c_uint32, c_void_p, c_uint32, c_size_t, c_uint32)
_sig(_lib.coda_table_row_remove, c_uint32, c_void_p, c_uint32, c_size_t)

_sig(_lib.coda_keyed_table_col_count,  c_size_t, c_void_p, c_uint32)
_sig(_lib.coda_keyed_table_col_name,   _CodaStr, c_void_p, c_uint32, c_size_t)
_sig(_lib.coda_keyed_table_col_append, c_uint32, c_void_p, c_uint32, c_char_p, c_size_t)
_sig(_lib.coda_keyed_table_row_count,  c_size_t, c_void_p, c_uint32)
_sig(_lib.coda_keyed_table_row_key_at, _CodaStr, c_void_p, c_uint32, c_size_t)
_sig(_lib.coda_keyed_table_row_at,     c_uint32, c_void_p, c_uint32, c_size_t)
_sig(_lib.coda_keyed_table_row_get,    c_uint32, c_void_p, c_uint32, c_char_p, c_size_t)
_sig(_lib.coda_keyed_table_row_set,    c_uint32, c_void_p, c_uint32, c_char_p, c_size_t, c_uint32)
_sig(_lib.coda_keyed_table_row_remove, c_uint32, c_void_p, c_uint32, c_char_p, c_size_t)

_sig(_lib.coda_row_get,          _CodaStr, c_void_p, c_uint32, c_char_p, c_size_t)
_sig(_lib.coda_row_set,          c_uint32, c_void_p, c_uint32, c_char_p, c_size_t, c_char_p, c_size_t)
_sig(_lib.coda_row_remove,       c_uint32, c_void_p, c_uint32, c_char_p, c_size_t)
_sig(_lib.coda_row_col_count,    c_size_t, c_void_p, c_uint32)
_sig(_lib.coda_row_col_name_at,  _CodaStr, c_void_p, c_uint32, c_size_t)
_sig(_lib.coda_row_col_value_at, _CodaStr, c_void_p, c_uint32, c_size_t)
_sig(_lib.coda_row_comment_get,  _CodaStr, c_void_p, c_uint32)
_sig(_lib.coda_row_comment_set,  c_uint32, c_void_p, c_uint32, c_char_p, c_size_t)

_sig(_lib.coda_new_string,      c_uint32, c_void_p, c_char_p, c_size_t)
_sig(_lib.coda_new_block,       c_uint32, c_void_p)
_sig(_lib.coda_new_array,       c_uint32, c_void_p)
_sig(_lib.coda_new_table,       c_uint32, c_void_p)
_sig(_lib.coda_new_keyed_table, c_uint32, c_void_p)
_sig(_lib.coda_new_row,         c_uint32, c_void_p)


# ─── Exceptions ───────────────────────────────────────────────────────────────

class CodaException(Exception):
	pass

class CodaParseError(CodaException):
	def __init__(self, message: str, code: int = 0, line: int = 0,
	             col: int = 0, offset: int = 0):
		super().__init__(message)
		self.code   = code
		self.line   = line
		self.col    = col
		self.offset = offset

	def __str__(self):
		if self.line > 0 and self.col > 0:
			return f"{super().__str__()} (line {self.line}, col {self.col})"
		return super().__str__()


# ─── Internal helpers ─────────────────────────────────────────────────────────

def _enc(s: str) -> bytes:
	return s.encode("utf-8")

def _node_from_id(doc: 'CodaDoc', node_id: int) -> 'CodaNode':
	"""Wrap a raw node_id in the correct Python class."""
	kind = _lib.coda_node_kind(doc._ptr, node_id)
	if kind == _NODE_STRING:
		return CodaString._wrap(doc, node_id)
	if kind == _NODE_BLOCK:
		return CodaBlock._wrap(doc, node_id)
	if kind == _NODE_ARRAY:
		return CodaArray._wrap(doc, node_id)
	if kind == _NODE_TABLE:
		return CodaTable._wrap(doc, node_id)
	if kind == _NODE_KEYED_TABLE:
		return CodaKeyedTable._wrap(doc, node_id)
	if kind == _NODE_ROW:
		return CodaRow._wrap(doc, node_id)
	raise CodaException(f"Unknown node kind: {kind}")


def _materialize(node: 'CodaNode', doc: 'CodaDoc') -> None:
	"""Ensure a node is allocated in *doc*, materializing it if it was created without one."""
	if node._doc is None:
		node._materialize(doc)


# ─── Base node ────────────────────────────────────────────────────────────────

class CodaNode:
	"""Base class for all Coda node types. Not instantiated directly."""

	__slots__ = ("_doc", "_node_id")

	_doc:     'Optional[CodaDoc]'
	_node_id: Optional[int]

	def __init__(self, doc: 'CodaDoc', node_id: int):
		self._doc     = doc
		self._node_id = node_id

	@classmethod
	def _wrap(cls, doc: 'CodaDoc', node_id: int) -> Self:
		"""Wrap an existing node_id without calling __init__."""
		obj = cls.__new__(cls)
		obj._doc     = doc
		obj._node_id = node_id
		return obj

	def _materialize(self, doc: 'CodaDoc') -> None:
		"""Allocate this pending node in *doc*. Subclasses override."""
		raise NotImplementedError(f"{type(self).__name__} does not support doc-less construction")

	def _check(self) -> 'CodaDoc':
		if self._doc is None:
			raise CodaException("CodaNode has not been attached to a document yet")
		if self._doc._ptr is None:
			raise CodaException("CodaDoc has been freed")
		return self._doc

	@property
	def comment(self) -> str:
		doc = self._check()
		return _lib.coda_node_comment_get(doc._ptr, self._node_id).to_python()

	@comment.setter
	def comment(self, value: str):
		doc = self._check()
		b = _enc(value)
		if _lib.coda_node_comment_set(doc._ptr, self._node_id, b, len(b)) != _CODA_OK:
			raise CodaException("Failed to set comment")

	def __repr__(self) -> str:
		return f"{type(self).__name__}(node_id={self._node_id})"

	def is_container(self) -> bool:
		"""Return True if this node is a container (Block, Array, Table, KeyedTable)."""
		doc = self._check()
		return bool(_lib.coda_node_is_container(doc._ptr, self._node_id))

	def serialize(self, indent: str = "\t") -> str:
		"""Serialize this node to Coda text."""
		doc = self._check()
		ib = _enc(indent)
		err = _CodaError()
		res = _lib.coda_node_serialize(doc._ptr, self._node_id, ib, len(ib), ctypes.byref(err))
		if res.ptr is None:
			msg = err.message.to_python()
			_lib.coda_error_clear(ctypes.byref(err))
			raise CodaException(f"Serialization failed: {msg}")
		out = res.to_python_and_free()
		return out

	def as_string(self) -> 'CodaString':
		"""Narrow this node to CodaString, raising TypeError if it is not one."""
		if not isinstance(self, CodaString):
			raise TypeError(f"Expected CodaString, got {type(self).__name__}")
		return self

	def as_block(self) -> 'CodaBlock':
		"""Narrow this node to CodaBlock, raising TypeError if it is not one."""
		if not isinstance(self, CodaBlock):
			raise TypeError(f"Expected CodaBlock, got {type(self).__name__}")
		return self

	def as_array(self) -> 'CodaArray':
		"""Narrow this node to CodaArray, raising TypeError if it is not one."""
		if not isinstance(self, CodaArray):
			raise TypeError(f"Expected CodaArray, got {type(self).__name__}")
		return self

	def as_table(self) -> 'CodaTable':
		"""Narrow this node to CodaTable, raising TypeError if it is not one."""
		if not isinstance(self, CodaTable):
			raise TypeError(f"Expected CodaTable, got {type(self).__name__}")
		return self

	def as_keyed_table(self) -> 'CodaKeyedTable':
		"""Narrow this node to CodaKeyedTable, raising TypeError if it is not one."""
		if not isinstance(self, CodaKeyedTable):
			raise TypeError(f"Expected CodaKeyedTable, got {type(self).__name__}")
		return self


# ─── CodaString ───────────────────────────────────────────────────────────────

class CodaString(CodaNode):
	"""
	A leaf string value.

    Mirrors: std::string inside coda::detail::Value.

        s = CodaString("hello")       # doc-less; materialized on insert
        s = CodaString(doc, "hello")  # legacy: allocate immediately
        s.value = "world"
        str(s)   # → "world"
	"""

	def __init__(self, value_or_doc: 'Union[str, CodaDoc]' = "", value: str = ""):
		if isinstance(value_or_doc, CodaDoc):
			# Legacy API: CodaString(doc, "value")
			doc = value_or_doc
			doc._check()
			b   = _enc(value)
			nid = _lib.coda_new_string(doc._ptr, b, len(b))
			if nid == 0:
				raise CodaException("Failed to create string node")
			super().__init__(doc, nid)
		else:
			# New API: CodaString("value") — pending until materialized
			self._doc           = None
			self._node_id       = None
			self._pending_value = value_or_doc if value_or_doc is not None else ""

	def _materialize(self, doc: 'CodaDoc') -> None:
		"""Allocate this node in *doc*. Called automatically on insert/append."""
		doc._check()
		b   = _enc(self._pending_value)
		nid = _lib.coda_new_string(doc._ptr, b, len(b))
		if nid == 0:
			raise CodaException("Failed to create string node")
		self._doc     = doc
		self._node_id = nid

	@property
	def value(self) -> str:
		doc = self._check()
		return _lib.coda_string_get(doc._ptr, self._node_id).to_python()

	@value.setter
	def value(self, v: str):
		doc = self._check()
		b = _enc(v)
		if _lib.coda_string_set(doc._ptr, self._node_id, b, len(b)) != _CODA_OK:
			raise CodaException("Failed to set string value")

	def __str__(self) -> str:
		return self.value

	def __eq__(self, other) -> bool:
		if isinstance(other, str):
			return self.value == other
		if isinstance(other, CodaString):
			return self.value == other.value
		return NotImplemented

	def __repr__(self) -> str:
		return f"CodaString({self.value!r})"


# ─── CodaRow ──────────────────────────────────────────────────────────────────

class CodaRow(CodaNode):
	"""
	A single row inside a CodaTable or CodaKeyedTable.
	Fields are flat string→string (no sub-nodes).

    Mirrors: coda::Row

        row = CodaRow()           # doc-less; materialized on insert
        row = CodaRow(doc)        # legacy
        row["col1"] = "value"
        row["col2"] = "other"
	"""

	def __init__(self, doc: 'Optional[CodaDoc]' = None):
		if doc is not None:
			# Legacy API: CodaRow(doc)
			doc._check()
			nid = _lib.coda_new_row(doc._ptr)
			if nid == 0:
				raise CodaException("Failed to create row node")
			super().__init__(doc, nid)
		else:
			# New API: CodaRow() — pending
			self._doc            = None
			self._node_id        = None
			self._pending_fields = {}  # ordered dict of field → value

	def _materialize(self, doc: 'CodaDoc') -> None:
		doc._check()
		nid = _lib.coda_new_row(doc._ptr)
		if nid == 0:
			raise CodaException("Failed to create row node")
		self._doc     = doc
		self._node_id = nid
		for col, val in self._pending_fields.items():
			cb, vb = _enc(col), _enc(val)
			_lib.coda_row_set(doc._ptr, nid, cb, len(cb), vb, len(vb))

	# CodaRow uses its own comment API, not coda_node_comment_*
	@property
	def comment(self) -> str:
		doc = self._check()
		return _lib.coda_row_comment_get(doc._ptr, self._node_id).to_python()

	@comment.setter
	def comment(self, value: str):
		doc = self._check()
		b = _enc(value)
		if _lib.coda_row_comment_set(doc._ptr, self._node_id, b, len(b)) != _CODA_OK:
			raise CodaException("Failed to set row comment")

	def __getitem__(self, col: str) -> str:
		doc = self._check()
		# Walk fields to verify the column exists (C returns "" for missing cols)
		n = _lib.coda_row_col_count(doc._ptr, self._node_id)
		for i in range(n):
			if _lib.coda_row_col_name_at(doc._ptr, self._node_id, i).to_python() == col:
				b = _enc(col)
				return _lib.coda_row_get(doc._ptr, self._node_id, b, len(b)).to_python()
		raise KeyError(col)

	def __setitem__(self, col: str, value: str):
		if self._doc is None:
			self._pending_fields[col] = value
			return
		doc = self._check()
		cb, vb = _enc(col), _enc(value)
		if _lib.coda_row_set(doc._ptr, self._node_id,
		                     cb, len(cb), vb, len(vb)) != _CODA_OK:
			raise CodaException(f"Failed to set row field: {col}")

	def __delitem__(self, col: str):
		doc = self._check()
		b  = _enc(col)
		st = _lib.coda_row_remove(doc._ptr, self._node_id, b, len(b))
		if st == _CODA_NOT_FOUND:
			raise KeyError(col)
		if st != _CODA_OK:
			raise CodaException(f"Failed to remove row field: {col}")

	def get(self, col: str, default: Optional[str] = None) -> Optional[str]:
		try:
			return self[col]
		except KeyError:
			return default

	def __contains__(self, col: str) -> bool:
		try:
			self[col]
			return True
		except KeyError:
			return False

	def __iter__(self) -> Iterator[Tuple[str, str]]:
		"""Yield (column_name, value) pairs in insertion order."""
		doc = self._check()
		n = _lib.coda_row_col_count(doc._ptr, self._node_id)
		for i in range(n):
			name  = _lib.coda_row_col_name_at(doc._ptr, self._node_id, i).to_python()
			value = _lib.coda_row_col_value_at(doc._ptr, self._node_id, i).to_python()
			yield name, value

	def __len__(self) -> int:
		doc = self._check()
		return _lib.coda_row_col_count(doc._ptr, self._node_id)

	def __repr__(self) -> str:
		return f"CodaRow({dict(self)!r})"


# ─── CodaBlock ────────────────────────────────────────────────────────────────

# Type alias for anything that can be a value inside a block or array
_AnyNode = Union['CodaString', 'CodaBlock', 'CodaArray', 'CodaTable', 'CodaKeyedTable']

class CodaBlock(CodaNode):
	"""
	A { key value ... } block.

    Mirrors: coda::Block

        block = CodaBlock()
        block.insert("name", CodaString("Alice"))
        block["age"] = CodaString("30")    # same as insert
	"""

	def __init__(self, doc: 'Optional[CodaDoc]' = None):
		if doc is not None:
			# Legacy API: CodaBlock(doc)
			doc._check()
			nid = _lib.coda_new_block(doc._ptr)
			if nid == 0:
				raise CodaException("Failed to create block node")
			super().__init__(doc, nid)
		else:
			# New API: CodaBlock() — pending
			self._doc     = None
			self._node_id = 0

	def _materialize(self, doc: 'CodaDoc') -> None:
		doc._check()
		nid = _lib.coda_new_block(doc._ptr)
		if nid == 0:
			raise CodaException("Failed to create block node")
		self._doc     = doc
		self._node_id = nid

	def insert(self, key: str, value: _AnyNode) -> _AnyNode:
		"""Insert (or replace) a child node under key. Returns the value."""
		doc = self._check()
		_materialize(value, doc)
		kb = _enc(key)
		if _lib.coda_map_set(doc._ptr, self._node_id,
		                     kb, len(kb), value._node_id) != _CODA_OK:
			raise CodaException(f"Failed to insert key: {key}")
		return value

	def __setitem__(self, key: str, value: _AnyNode):
		self.insert(key, value)

	def __getitem__(self, key: str) -> CodaNode:
		doc = self._check()
		kb       = _enc(key)
		child_id = _lib.coda_map_get(doc._ptr, self._node_id, kb, len(kb))
		if child_id == 0:
			raise KeyError(key)
		return _node_from_id(doc, child_id)

	def __delitem__(self, key: str):
		doc = self._check()
		kb = _enc(key)
		st = _lib.coda_map_remove(doc._ptr, self._node_id, kb, len(kb))
		if st == _CODA_NOT_FOUND:
			raise KeyError(key)
		if st != _CODA_OK:
			raise CodaException(f"Failed to remove key: {key}")

	def __contains__(self, key: str) -> bool:
		doc = self._check()
		kb = _enc(key)
		return _lib.coda_map_get(doc._ptr, self._node_id, kb, len(kb)) != 0

	def has(self, key: str) -> bool:
		"""Return True if the block contains the given key."""
		return key in self

	def __iter__(self) -> Iterator[Tuple[str, CodaNode]]:
		"""Yield (key, node) pairs in insertion order."""
		doc = self._check()
		n = _lib.coda_map_len(doc._ptr, self._node_id)
		for i in range(n):
			key      = _lib.coda_map_key_at(doc._ptr, self._node_id, i).to_python()
			value_id = _lib.coda_map_value_at(doc._ptr, self._node_id, i)
			yield key, _node_from_id(doc, value_id)

	def __len__(self) -> int:
		doc = self._check()
		return _lib.coda_map_len(doc._ptr, self._node_id)

	def get_or_insert(self, key: str) -> CodaNode:
		"""Return the node for key, inserting an empty CodaString if absent."""
		doc = self._check()
		kb       = _enc(key)
		child_id = _lib.coda_map_get_or_insert(doc._ptr, self._node_id, kb, len(kb))
		if child_id == 0:
			raise CodaException(f"Failed to get_or_insert key: {key}")
		return _node_from_id(doc, child_id)

	def order(self) -> None:
		"""Reorder keys: scalars first, then containers; alphabetical within groups."""
		doc = self._check()
		_lib.coda_node_order(doc._ptr, self._node_id)

	def order_weighted(self, weights: list[tuple[str, float]]) -> None:
		"""Reorder keys by weight (higher weight → closer to top)."""
		doc = self._check()
		if not weights:
			_lib.coda_node_order(doc._ptr, self._node_id)
			return
		keys = (c_char_p * len(weights))(*[_enc(k) for k, _ in weights])
		vals = (ctypes.c_float * len(weights))(*[v for _, v in weights])
		_lib.coda_node_order_weighted(doc._ptr, self._node_id, keys, vals, len(weights))


# ─── CodaArray ────────────────────────────────────────────────────────────────

class CodaArray(CodaNode):
	"""
	A [ ... ] array of ordered child nodes.

    Mirrors: coda::Array

        arr = CodaArray()
        arr.append(CodaString("item"))
        arr.append(CodaBlock())
	"""

	def __init__(self, doc: 'Optional[CodaDoc]' = None):
		if doc is not None:
			# Legacy API: CodaArray(doc)
			doc._check()
			nid = _lib.coda_new_array(doc._ptr)
			if nid == 0:
				raise CodaException("Failed to create array node")
			super().__init__(doc, nid)
		else:
			# New API: CodaArray() — pending
			self._doc     = None
			self._node_id = 0

	def _materialize(self, doc: 'CodaDoc') -> None:
		doc._check()
		nid = _lib.coda_new_array(doc._ptr)
		if nid == 0:
			raise CodaException("Failed to create array node")
		self._doc     = doc
		self._node_id = nid

	@property
	def header_comment(self) -> str:
		doc = self._check()
		return _lib.coda_node_header_comment_get(doc._ptr, self._node_id).to_python()

	@header_comment.setter
	def header_comment(self, value: str):
		doc = self._check()
		b  = _enc(value)
		st = _lib.coda_node_header_comment_set(doc._ptr, self._node_id, b, len(b))
		if st == _CODA_BAD_KIND:
			raise TypeError("header_comment is only valid on array or table nodes")
		if st != _CODA_OK:
			raise CodaException("Failed to set header_comment")

	def append(self, value: _AnyNode) -> _AnyNode:
		"""Append a child node. Returns the value."""
		doc = self._check()
		_materialize(value, doc)
		if _lib.coda_array_push(doc._ptr, self._node_id, value._node_id) != _CODA_OK:
			raise CodaException("Failed to append to array")
		return value

	def __getitem__(self, idx: int) -> CodaNode:
		doc = self._check()
		n = _lib.coda_array_len(doc._ptr, self._node_id)
		i = idx if idx >= 0 else idx + n
		if i < 0 or i >= n:
			raise IndexError("array index out of range")
		child_id = _lib.coda_array_get(doc._ptr, self._node_id, i)
		if child_id == 0:
			raise IndexError("array element missing")
		return _node_from_id(doc, child_id)

	def __setitem__(self, idx: int, value: _AnyNode):
		doc = self._check()
		n = _lib.coda_array_len(doc._ptr, self._node_id)
		i = idx if idx >= 0 else idx + n
		if i < 0 or i >= n:
			raise IndexError("array index out of range")
		_materialize(value, doc)
		if _lib.coda_array_set(doc._ptr, self._node_id, i, value._node_id) != _CODA_OK:
			raise CodaException("Failed to set array element")

	def __delitem__(self, idx: int):
		doc = self._check()
		n = _lib.coda_array_len(doc._ptr, self._node_id)
		i = idx if idx >= 0 else idx + n
		if i < 0 or i >= n:
			raise IndexError("array index out of range")
		if _lib.coda_array_remove(doc._ptr, self._node_id, i) != _CODA_OK:
			raise CodaException("Failed to remove array element")

	def __iter__(self) -> Iterator[CodaNode]:
		doc = self._check()
		n = _lib.coda_array_len(doc._ptr, self._node_id)
		for i in range(n):
			child_id = _lib.coda_array_get(doc._ptr, self._node_id, i)
			if child_id != 0:
				yield _node_from_id(doc, child_id)

	def __len__(self) -> int:
		doc = self._check()
		return _lib.coda_array_len(doc._ptr, self._node_id)


# ─── CodaTable ────────────────────────────────────────────────────────────────

class CodaTable(CodaNode):
	"""
	An anonymous-row plain table.

    Mirrors: coda::Table

        t = CodaTable(["col1", "col2"])
        row = CodaRow()
        row["col1"] = "a"
        row["col2"] = "b"
        t.append(row)

        for row in t:           # yields CodaRow
            print(row["col1"])
        t[0]["col1"]            # index access
	"""

	def __init__(self, doc_or_columns: 'Union[CodaDoc, list[str], None]' = None,
	             columns: 'list[str]' = []):
		if isinstance(doc_or_columns, CodaDoc):
			# Legacy API: CodaTable(doc, columns)
			doc = doc_or_columns
			cols = columns
			doc._check()
			nid = _lib.coda_new_table(doc._ptr)
			if nid == 0:
				raise CodaException("Failed to create table node")
			super().__init__(doc, nid)
			for col in cols:
				self.append_col(col)
		else:
			# New API: CodaTable(["col1", "col2"]) or CodaTable()
			self._doc              = None
			self._node_id          = None
			self._pending_columns  = doc_or_columns if isinstance(doc_or_columns, list) else []

	def _materialize(self, doc: 'CodaDoc') -> None:
		doc._check()
		nid = _lib.coda_new_table(doc._ptr)
		if nid == 0:
			raise CodaException("Failed to create table node")
		self._doc     = doc
		self._node_id = nid
		for col in self._pending_columns:
			self.append_col(col)

	@property
	def header_comment(self) -> str:
		doc = self._check()
		return _lib.coda_node_header_comment_get(doc._ptr, self._node_id).to_python()

	@header_comment.setter
	def header_comment(self, value: str):
		doc = self._check()
		b = _enc(value)
		if _lib.coda_node_header_comment_set(doc._ptr, self._node_id, b, len(b)) != _CODA_OK:
			raise CodaException("Failed to set header_comment")

	def columns(self) -> list[str]:
		doc = self._check()
		n = _lib.coda_table_col_count(doc._ptr, self._node_id)
		return [_lib.coda_table_col_name(doc._ptr, self._node_id, i).to_python()
		        for i in range(n)]

	def append_col(self, name: str) -> 'CodaTable':
		"""Append a column name. Returns self for chaining."""
		doc = self._check()
		b = _enc(name)
		if _lib.coda_table_col_append(doc._ptr, self._node_id, b, len(b)) != _CODA_OK:
			raise CodaException(f"Failed to append column: {name}")
		return self

	def append(self, row: CodaRow) -> 'CodaTable':
		"""Append a CodaRow. Returns self for chaining."""
		doc = self._check()
		_materialize(row, doc)
		if _lib.coda_table_row_append(doc._ptr, self._node_id, row._node_id) != _CODA_OK:
			raise CodaException("Failed to append row")
		return self

	def __getitem__(self, idx: int) -> CodaRow:
		doc = self._check()
		n = _lib.coda_table_row_count(doc._ptr, self._node_id)
		i = idx if idx >= 0 else idx + n
		if i < 0 or i >= n:
			raise IndexError("table row index out of range")
		row_id = _lib.coda_table_row_at(doc._ptr, self._node_id, i)
		if row_id == 0:
			raise IndexError("table row missing")
		return CodaRow._wrap(doc, row_id)

	def __setitem__(self, idx: int, row: CodaRow):
		doc = self._check()
		n = _lib.coda_table_row_count(doc._ptr, self._node_id)
		i = idx if idx >= 0 else idx + n
		if i < 0 or i >= n:
			raise IndexError("table row index out of range")
		_materialize(row, doc)
		if _lib.coda_table_row_set(doc._ptr, self._node_id, i, row._node_id) != _CODA_OK:
			raise CodaException("Failed to set table row")

	def __delitem__(self, idx: int):
		doc = self._check()
		n = _lib.coda_table_row_count(doc._ptr, self._node_id)
		i = idx if idx >= 0 else idx + n
		if i < 0 or i >= n:
			raise IndexError("table row index out of range")
		if _lib.coda_table_row_remove(doc._ptr, self._node_id, i) != _CODA_OK:
			raise CodaException("Failed to remove table row")

	def __iter__(self) -> Iterator[CodaRow]:
		doc = self._check()
		n = _lib.coda_table_row_count(doc._ptr, self._node_id)
		for i in range(n):
			row_id = _lib.coda_table_row_at(doc._ptr, self._node_id, i)
			if row_id != 0:
				yield CodaRow._wrap(doc, row_id)

	def __len__(self) -> int:
		doc = self._check()
		return _lib.coda_table_row_count(doc._ptr, self._node_id)


# ─── CodaKeyedTable ───────────────────────────────────────────────────────────

class CodaKeyedTable(CodaNode):
	"""
	A key-indexed table.

    Mirrors: coda::KeyedTable

        kt = CodaKeyedTable(["col1", "col2"])
        row = CodaRow()
        row["col1"] = "a"
        row["col2"] = "b"
        kt.insert("mykey", row)

        for key, row in kt:     # yields (str, CodaRow)
            print(key, row["col1"])
        kt["mykey"]["col1"]     # key + field access
	"""

	def __init__(self, doc_or_columns: 'Union[CodaDoc, list[str], None]' = None,
	             columns: 'list[str]' = []):
		if isinstance(doc_or_columns, CodaDoc):
			# Legacy API: CodaKeyedTable(doc, columns)
			doc = doc_or_columns
			cols = columns
			doc._check()
			nid = _lib.coda_new_keyed_table(doc._ptr)
			if nid == 0:
				raise CodaException("Failed to create keyed table node")
			super().__init__(doc, nid)
			for col in cols:
				self.append_col(col)
		else:
			# New API: CodaKeyedTable(["col1", "col2"]) or CodaKeyedTable()
			self._doc             = None
			self._node_id         = None
			self._pending_columns = doc_or_columns if isinstance(doc_or_columns, list) else []

	def _materialize(self, doc: 'CodaDoc') -> None:
		doc._check()
		nid = _lib.coda_new_keyed_table(doc._ptr)
		if nid == 0:
			raise CodaException("Failed to create keyed table node")
		self._doc     = doc
		self._node_id = nid
		for col in self._pending_columns:
			self.append_col(col)

	@property
	def header_comment(self) -> str:
		doc = self._check()
		return _lib.coda_node_header_comment_get(doc._ptr, self._node_id).to_python()

	@header_comment.setter
	def header_comment(self, value: str):
		doc = self._check()
		b = _enc(value)
		if _lib.coda_node_header_comment_set(doc._ptr, self._node_id, b, len(b)) != _CODA_OK:
			raise CodaException("Failed to set header_comment")

	def columns(self) -> list[str]:
		doc = self._check()
		n = _lib.coda_keyed_table_col_count(doc._ptr, self._node_id)
		return [_lib.coda_keyed_table_col_name(doc._ptr, self._node_id, i).to_python()
		        for i in range(n)]

	def append_col(self, name: str) -> 'CodaKeyedTable':
		"""Append a column name. Returns self for chaining."""
		doc = self._check()
		b = _enc(name)
		if _lib.coda_keyed_table_col_append(doc._ptr, self._node_id, b, len(b)) != _CODA_OK:
			raise CodaException(f"Failed to append column: {name}")
		return self

	def insert(self, key: str, row: CodaRow) -> CodaRow:
		"""Insert or replace a row by key. Returns the row."""
		doc = self._check()
		_materialize(row, doc)
		kb = _enc(key)
		if _lib.coda_keyed_table_row_set(doc._ptr, self._node_id,
		                                 kb, len(kb), row._node_id) != _CODA_OK:
			raise CodaException(f"Failed to insert row: {key}")
		return row

	def __setitem__(self, key: str, row: CodaRow):
		self.insert(key, row)

	def __getitem__(self, key: str) -> CodaRow:
		doc = self._check()
		kb     = _enc(key)
		row_id = _lib.coda_keyed_table_row_get(doc._ptr, self._node_id, kb, len(kb))
		if row_id == 0:
			raise KeyError(key)
		return CodaRow._wrap(doc, row_id)

	def __delitem__(self, key: str):
		doc = self._check()
		kb = _enc(key)
		st = _lib.coda_keyed_table_row_remove(doc._ptr, self._node_id, kb, len(kb))
		if st == _CODA_NOT_FOUND:
			raise KeyError(key)
		if st != _CODA_OK:
			raise CodaException(f"Failed to remove row: {key}")

	def __contains__(self, key: str) -> bool:
		doc = self._check()
		kb = _enc(key)
		return _lib.coda_keyed_table_row_get(doc._ptr, self._node_id, kb, len(kb)) != 0

	def __iter__(self) -> Iterator[Tuple[str, CodaRow]]:
		"""Yield (key, CodaRow) pairs in insertion order."""
		doc = self._check()
		n = _lib.coda_keyed_table_row_count(doc._ptr, self._node_id)
		for i in range(n):
			key    = _lib.coda_keyed_table_row_key_at(doc._ptr, self._node_id, i).to_python()
			row_id = _lib.coda_keyed_table_row_at(doc._ptr, self._node_id, i)
			if row_id != 0:
				yield key, CodaRow._wrap(doc, row_id)

	def __len__(self) -> int:
		doc = self._check()
		return _lib.coda_keyed_table_row_count(doc._ptr, self._node_id)

	def order(self) -> None:
		"""Reorder rows: alphabetically by key."""
		doc = self._check()
		_lib.coda_node_order(doc._ptr, self._node_id)

	def order_weighted(self, weights: list[tuple[str, float]]) -> None:
		"""Reorder rows by weight."""
		doc = self._check()
		if not weights:
			_lib.coda_node_order(doc._ptr, self._node_id)
			return
		keys = (c_char_p * len(weights))(*[_enc(k) for k, _ in weights])
		vals = (ctypes.c_float * len(weights))(*[v for _, v in weights])
		_lib.coda_node_order_weighted(doc._ptr, self._node_id, keys, vals, len(weights))


# ─── CodaDoc ──────────────────────────────────────────────────────────────────

class CodaDoc:
	"""
	The document arena. Owns all node memory.

    All CodaNode objects produced from a doc share its arena and become
    invalid after CodaDoc.free() / exiting the context manager.

        with CodaDoc.parse(text) as doc:
            root = doc.root()
            ...

        doc = CodaDoc.new()
        root = doc.root()
        root.insert("key", CodaString("value"))
        doc.save("out.coda")
        doc.free()
	"""

	def __init__(self, ptr):
		self._ptr = ptr

	def _check(self) -> 'CodaDoc':
		if self._ptr is None:
			raise CodaException("CodaDoc has been freed")
		return self

	# ── Lifecycle ─────────────────────────────────────────────────────────────

	@classmethod
	def parse(cls, text: str, filename: Optional[str] = None) -> 'CodaDoc':
		tb  = text.encode("utf-8")
		fb  = filename.encode("utf-8") if filename else None
		err = _CodaError()
		ptr = _lib.coda_doc_parse(tb, len(tb), fb, ctypes.byref(err))
		if ptr is None:
			msg, code, line, col, offset = (err.message.to_python(), err.code,
			                                err.line, err.col, err.offset)
			_lib.coda_error_clear(ctypes.byref(err))
			raise CodaParseError(msg, code=code, line=line, col=col, offset=offset)
		return cls(ptr)

	@classmethod
	def parse_file(cls, path: str) -> 'CodaDoc':
		pb  = path.encode("utf-8")
		err = _CodaError()
		ptr = _lib.coda_doc_parse_file(pb, ctypes.byref(err))
		if ptr is None:
			msg, code, line, col, offset = (err.message.to_python(), err.code,
			                                err.line, err.col, err.offset)
			_lib.coda_error_clear(ctypes.byref(err))
			raise CodaParseError(msg, code=code, line=line, col=col, offset=offset)
		return cls(ptr)

	@classmethod
	def new(cls) -> 'CodaDoc':
		"""Create a new empty document."""
		ptr = _lib.coda_doc_new()
		if ptr is None:
			raise CodaException("Failed to create document")
		return cls(ptr)

	def __enter__(self) -> 'CodaDoc':
		return self

	def __exit__(self, *_):
		self.free()
		return False

	def close(self):
		"""Idempotent alias for free(). Suitable for use with contextlib.closing()."""
		self.free()

	def free(self):
		if self._ptr is not None:
			_lib.coda_doc_free(self._ptr)
			self._ptr = None

	def __del__(self):
		# Backstop: catches cases where the context manager or close() was not used.
		# Do not rely on this for correctness — always prefer the context manager.
		try:
			self.free()
		except Exception:
			pass

	# ── Root access ───────────────────────────────────────────────────────────

	def root(self) -> CodaBlock:
		"""Return the root CodaBlock node."""
		doc = self._check()
		return CodaBlock._wrap(self, _lib.coda_doc_root(doc._ptr))

	# ── Serialisation ─────────────────────────────────────────────────────────

	def serialize(self, indent: str = "\t") -> str:
		doc = self._check()
		ib  = indent.encode("utf-8")
		err = _CodaError()
		res = _lib.coda_doc_serialize(self._ptr, ib, len(ib), ctypes.byref(err))
		if res.ptr is None:
			msg = err.message.to_python()
			_lib.coda_error_clear(ctypes.byref(err))
			raise CodaException(f"Serialization failed: {msg}")
		text = res.to_python()
		_lib.coda_owned_str_free(res)
		return text

	def save(self, path: str, indent: str = "\t"):
		with open(path, "w", encoding="utf-8") as f:
			f.write(self.serialize(indent))

	def order(self) -> None:
		"""Reorder all keys: scalars first, then containers; alphabetical."""
		doc = self._check()
		_lib.coda_doc_order(self._ptr)

	def order_weighted(self, weights: list[tuple[str, float]]) -> None:
		"""Reorder keys by weight (higher weight → closer to top)."""
		doc = self._check()
		if not weights:
			_lib.coda_doc_order(self._ptr)
			return
		keys = (c_char_p * len(weights))()
		vals = (ctypes.c_float * len(weights))()
		for i, (k, v) in enumerate(weights):
			keys[i] = k.encode("utf-8")
			vals[i] = v
		_lib.coda_doc_order_weighted(self._ptr, keys, vals, len(weights))

	def order_weighted_and_serialize(self, weights: list[tuple[str, float]],
	                                 indent: str = "\t") -> str:
		self.order_weighted(weights)
		return self.serialize(indent)


# ─── Module-level utilities ───────────────────────────────────────────────────

def get_abi_version() -> int:
	return _lib.coda_ffi_abi_version()


__all__ = [
	"CodaDoc",
	"CodaBlock",
	"CodaArray",
	"CodaTable",
	"CodaKeyedTable",
	"CodaRow",
	"CodaString",
	"CodaException",
	"CodaParseError",
	"get_abi_version",
]
