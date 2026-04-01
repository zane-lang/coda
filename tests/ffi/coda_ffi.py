"""
Python FFI bindings for Coda configuration format parser.

This module provides a Pythonic interface to the Coda C library using ctypes.
"""

import ctypes
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
        length = _lib.coda_array_len(self._doc._ptr, self._node_id)
        for i in range(length):
            child_id = _lib.coda_array_get(self._doc._ptr, self._node_id, i)
            if child_id != 0:
                yield Coda(self._doc, child_id)
    
    def asBlock(self) -> Iterator[Tuple[str, 'Coda']]:
        """Iterate over block/map key-value pairs."""
        self._check_valid()
        length = _lib.coda_map_len(self._doc._ptr, self._node_id)
        for i in range(length):
            key = _lib.coda_map_key_at(self._doc._ptr, self._node_id, i)
            value_id = _lib.coda_map_value_at(self._doc._ptr, self._node_id, i)
            if value_id != 0:
                yield (key.to_python(), Coda(self._doc, value_id))
    
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
    
    def __enter__(self):
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
