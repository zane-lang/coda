"""Runtime safety checks layered over the ctypes binding.

The native FFI rejects stale, cross-document, multiply-parented and cyclic node
handles. These hooks turn those status failures into clear Python exceptions
before callers accidentally treat an empty native result as a real value.
"""

from __future__ import annotations

from . import coda as _coda


_original_node_check = _coda.Node._check


def _checked_node(self: _coda.Node) -> _coda.Doc:
	doc = _original_node_check(self)
	if self._node_id is None or _coda._lib.coda_node_kind(doc._ptr, self._node_id) == _coda._NODE_NULL:
		raise _coda.Error("Node handle is stale or invalid")
	return doc


def _safe_materialize(node: _coda.Node, doc: _coda.Doc) -> None:
	if node._doc is None:
		node._materialize(doc)
	elif node._doc is not doc:
		raise _coda.Error("Cannot attach a node from another document")


def apply() -> None:
	_coda.Node._check = _checked_node
	# Block/Array/Table/KeyedTable methods resolve this module global at call time.
	_coda._materialize = _safe_materialize
