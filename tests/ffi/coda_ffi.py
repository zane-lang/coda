import os
import sys
import ctypes
from ctypes import (
	c_void_p, c_char_p, c_size_t, c_uint32, c_uint8, c_int,
	Structure, POINTER, byref
)

# ----------------------------
# Load the library
# ----------------------------

def _load_lib():
	here = os.path.abspath(os.path.dirname(__file__))

	if sys.platform.startswith("linux"):
		name = "libcoda_ffi.so"
	elif sys.platform == "darwin":
		name = "libcoda_ffi.dylib"
	elif sys.platform == "win32":
		name = "libcoda_ffi.dll"
		# Optional on Windows if DLL dependencies live next to it:
		# os.add_dll_directory(here)
	else:
		raise RuntimeError(f"unsupported platform: {sys.platform}")

	return ctypes.CDLL(name)

lib = _load_lib()

# ----------------------------
# ctypes representations of your C structs
# ----------------------------

class coda_str_t(Structure):
	_fields_ = [
		("ptr", c_void_p),   # NOT c_char_p (not necessarily NUL-terminated)
		("len", c_size_t),
	]

class coda_owned_str_t(Structure):
	_fields_ = [
		("ptr", c_void_p),   # char*
		("len", c_size_t),
	]

class coda_error_t(Structure):
	_fields_ = [
		("code", c_uint32),
		("line", c_uint32),
		("col",  c_uint32),
		("offset", c_size_t),
		("message", coda_owned_str_t),
	]

coda_doc_p  = c_void_p      # opaque pointer
coda_node_t = c_uint32      # handle

# ----------------------------
# Function prototypes (argtypes/restype)
# ----------------------------

lib.coda_error_clear.argtypes = [POINTER(coda_error_t)]
lib.coda_error_clear.restype  = None

lib.coda_owned_str_free.argtypes = [coda_owned_str_t]
lib.coda_owned_str_free.restype  = None

lib.coda_doc_free.argtypes = [coda_doc_p]
lib.coda_doc_free.restype  = None

lib.coda_doc_parse.argtypes = [c_char_p, c_size_t, c_char_p, POINTER(coda_error_t)]
lib.coda_doc_parse.restype  = coda_doc_p

lib.coda_doc_root.argtypes = [coda_doc_p]
lib.coda_doc_root.restype  = coda_node_t

lib.coda_map_get.argtypes = [coda_doc_p, coda_node_t, c_char_p, c_size_t]
lib.coda_map_get.restype  = coda_node_t

lib.coda_string_get.argtypes = [coda_doc_p, coda_node_t]
lib.coda_string_get.restype  = coda_str_t

lib.coda_doc_serialize.argtypes = [coda_doc_p, c_char_p, c_size_t, POINTER(coda_error_t)]
lib.coda_doc_serialize.restype  = coda_owned_str_t

# ----------------------------
# Helpers
# ----------------------------

def _bytes_from_strview(v: coda_str_t) -> bytes:
	if not v.ptr:
		return b""
	# ctypes.string_at(address, size) copies bytes from memory. <!--citation:1-->
	return ctypes.string_at(v.ptr, v.len)

def _bytes_from_owned(v: coda_owned_str_t) -> bytes:
	if not v.ptr:
		return b""
	return ctypes.string_at(v.ptr, v.len)

# ----------------------------
# High-level convenience API (optional)
# ----------------------------

class CodaDoc:
	def __init__(self, doc_ptr):
		self._doc = doc_ptr

	@classmethod
	def parse(cls, text: str, filename: str = "input.coda") -> CodaDoc:
		err = coda_error_t()
		data = text.encode("utf-8")
		doc = lib.coda_doc_parse(data, len(data), filename.encode("utf-8"), byref(err))
		if not doc:
			msg = _bytes_from_owned(err.message).decode("utf-8", errors="replace")
			lib.coda_error_clear(byref(err))
			raise RuntimeError(f"parse failed ({err.line}:{err.col}): {msg}")
		lib.coda_error_clear(byref(err))
		return cls(doc)

	def close(self):
		if self._doc:
			lib.coda_doc_free(self._doc)
			self._doc = None

	def root(self) -> int:
		return int(lib.coda_doc_root(self._doc))

	def get_string(self, key: str) -> str:
		root = lib.coda_doc_root(self._doc)
		k = key.encode("utf-8")
		node = lib.coda_map_get(self._doc, root, k, len(k))
		if node == 0:
			raise KeyError(key)
		sv = lib.coda_string_get(self._doc, node)
		return _bytes_from_strview(sv).decode("utf-8")

	def serialize(self, indent: str = "\t") -> str:
		err = coda_error_t()
		ind = indent.encode("utf-8")
		out = lib.coda_doc_serialize(self._doc, ind, len(ind), byref(err))
		if not out.ptr:
			msg = _bytes_from_owned(err.message).decode("utf-8", errors="replace")
			lib.coda_error_clear(byref(err))
			raise RuntimeError(f"serialize failed: {msg}")
		try:
			s = _bytes_from_owned(out).decode("utf-8", errors="replace")
		finally:
			lib.coda_owned_str_free(out)
			lib.coda_error_clear(byref(err))
		return s

	def __enter__(self): return self
	def __exit__(self, *exc): self.close()
