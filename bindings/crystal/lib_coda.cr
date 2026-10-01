# The C FFI (ffi/coda_ffi.h), declared one-to-one. `coda.cr` builds the
# object API on top of it; most programs never call these directly.
#
# The program links the static archive `libcoda_ffi.a` (see docs/API-CRYSTAL.md
# for building it) and the C++ runtime the archive was compiled against.
# Crystal hands `Link` annotations to the linker in reverse order, and the C++
# runtime has to follow the archive, so it is declared first.
{% if flag?(:darwin) %}
  @[Link("c++")]
{% elsif !flag?(:win32) %}
  @[Link("stdc++")]
{% end %}
@[Link("coda_ffi")]
lib LibCoda
  alias Doc = Void*
  alias Node = UInt32

  struct Str
    ptr : UInt8*
    len : LibC::SizeT
  end

  struct OwnedStr
    ptr : UInt8*
    len : LibC::SizeT
  end

  enum NodeKind : UInt32
    Null       = 0
    String     = 2
    Block      = 3
    Array      = 4
    Table      = 5
    KeyedTable = 6
    Row        = 7
  end

  enum Status : UInt32
    Ok         = 0
    Err        = 1
    NotFound   = 2
    BadKind    = 3
    OutOfRange = 4
  end

  struct Error
    code : UInt32
    line : UInt32
    col : UInt32
    offset : LibC::SizeT
    message : OwnedStr
  end

  fun parse_error_code_name = coda_parse_error_code_name(code : UInt32) : Str

  fun free = coda_free(p : Void*)
  fun error_clear = coda_error_clear(err : Error*)
  fun owned_str_free = coda_owned_str_free(s : OwnedStr)
  fun ffi_abi_version = coda_ffi_abi_version : UInt32

  fun doc_new = coda_doc_new : Doc
  fun doc_free = coda_doc_free(doc : Doc)
  fun doc_parse = coda_doc_parse(src : UInt8*, len : LibC::SizeT, filename : UInt8*, err : Error*) : Doc
  fun doc_parse_file = coda_doc_parse_file(path : UInt8*, err : Error*) : Doc
  fun doc_serialize = coda_doc_serialize(doc : Doc, indent_unit : UInt8*, indent_unit_len : LibC::SizeT, err : Error*) : OwnedStr
  fun doc_order = coda_doc_order(doc : Doc)
  fun doc_order_weighted = coda_doc_order_weighted(doc : Doc, keys : UInt8**, weights : Float32*, count : LibC::SizeT)
  fun doc_root = coda_doc_root(doc : Doc) : Node

  fun node_kind = coda_node_kind(doc : Doc, n : Node) : NodeKind
  fun node_is_container = coda_node_is_container(doc : Doc, n : Node) : LibC::Int
  fun node_comment_get = coda_node_comment_get(doc : Doc, n : Node) : Str
  fun node_comment_set = coda_node_comment_set(doc : Doc, n : Node, s : UInt8*, len : LibC::SizeT) : Status
  fun node_header_comment_get = coda_node_header_comment_get(doc : Doc, n : Node) : Str
  fun node_header_comment_set = coda_node_header_comment_set(doc : Doc, n : Node, s : UInt8*, len : LibC::SizeT) : Status

  fun string_get = coda_string_get(doc : Doc, n : Node) : Str
  fun string_set = coda_string_set(doc : Doc, n : Node, s : UInt8*, len : LibC::SizeT) : Status

  fun array_len = coda_array_len(doc : Doc, a : Node) : LibC::SizeT
  fun array_get = coda_array_get(doc : Doc, a : Node, idx : LibC::SizeT) : Node
  fun array_set = coda_array_set(doc : Doc, a : Node, idx : LibC::SizeT, value : Node) : Status
  fun array_push = coda_array_push(doc : Doc, a : Node, value : Node) : Status
  fun array_remove = coda_array_remove(doc : Doc, a : Node, idx : LibC::SizeT) : Status

  fun map_len = coda_map_len(doc : Doc, m : Node) : LibC::SizeT
  fun map_key_at = coda_map_key_at(doc : Doc, m : Node, idx : LibC::SizeT) : Str
  fun map_value_at = coda_map_value_at(doc : Doc, m : Node, idx : LibC::SizeT) : Node
  fun map_get = coda_map_get(doc : Doc, m : Node, key : UInt8*, key_len : LibC::SizeT) : Node
  fun map_get_or_insert = coda_map_get_or_insert(doc : Doc, m : Node, key : UInt8*, key_len : LibC::SizeT) : Node
  fun map_set = coda_map_set(doc : Doc, m : Node, key : UInt8*, key_len : LibC::SizeT, value : Node) : Status
  fun map_remove = coda_map_remove(doc : Doc, m : Node, key : UInt8*, key_len : LibC::SizeT) : Status

  fun table_col_count = coda_table_col_count(doc : Doc, t : Node) : LibC::SizeT
  fun table_col_name = coda_table_col_name(doc : Doc, t : Node, col_idx : LibC::SizeT) : Str
  fun table_col_append = coda_table_col_append(doc : Doc, t : Node, name : UInt8*, name_len : LibC::SizeT) : Status
  fun table_row_count = coda_table_row_count(doc : Doc, t : Node) : LibC::SizeT
  fun table_row_at = coda_table_row_at(doc : Doc, t : Node, row_idx : LibC::SizeT) : Node
  fun table_row_append = coda_table_row_append(doc : Doc, t : Node, row : Node) : Status
  fun table_row_set = coda_table_row_set(doc : Doc, t : Node, row_idx : LibC::SizeT, row : Node) : Status
  fun table_row_remove = coda_table_row_remove(doc : Doc, t : Node, row_idx : LibC::SizeT) : Status

  fun keyed_table_col_count = coda_keyed_table_col_count(doc : Doc, kt : Node) : LibC::SizeT
  fun keyed_table_col_name = coda_keyed_table_col_name(doc : Doc, kt : Node, col_idx : LibC::SizeT) : Str
  fun keyed_table_col_append = coda_keyed_table_col_append(doc : Doc, kt : Node, name : UInt8*, name_len : LibC::SizeT) : Status
  fun keyed_table_row_count = coda_keyed_table_row_count(doc : Doc, kt : Node) : LibC::SizeT
  fun keyed_table_row_key_at = coda_keyed_table_row_key_at(doc : Doc, kt : Node, row_idx : LibC::SizeT) : Str
  fun keyed_table_row_at = coda_keyed_table_row_at(doc : Doc, kt : Node, row_idx : LibC::SizeT) : Node
  fun keyed_table_row_get = coda_keyed_table_row_get(doc : Doc, kt : Node, key : UInt8*, key_len : LibC::SizeT) : Node
  fun keyed_table_row_set = coda_keyed_table_row_set(doc : Doc, kt : Node, key : UInt8*, key_len : LibC::SizeT, row : Node) : Status
  fun keyed_table_row_remove = coda_keyed_table_row_remove(doc : Doc, kt : Node, key : UInt8*, key_len : LibC::SizeT) : Status

  fun row_get = coda_row_get(doc : Doc, row : Node, col : UInt8*, col_len : LibC::SizeT) : Str
  fun row_set = coda_row_set(doc : Doc, row : Node, col : UInt8*, col_len : LibC::SizeT, val : UInt8*, val_len : LibC::SizeT) : Status
  fun row_remove = coda_row_remove(doc : Doc, row : Node, col : UInt8*, col_len : LibC::SizeT) : Status
  fun row_col_count = coda_row_col_count(doc : Doc, row : Node) : LibC::SizeT
  fun row_col_name_at = coda_row_col_name_at(doc : Doc, row : Node, idx : LibC::SizeT) : Str
  fun row_col_value_at = coda_row_col_value_at(doc : Doc, row : Node, idx : LibC::SizeT) : Str
  fun row_comment_get = coda_row_comment_get(doc : Doc, row : Node) : Str
  fun row_comment_set = coda_row_comment_set(doc : Doc, row : Node, s : UInt8*, len : LibC::SizeT) : Status

  fun node_serialize = coda_node_serialize(doc : Doc, n : Node, indent_unit : UInt8*, indent_unit_len : LibC::SizeT, err : Error*) : OwnedStr
  fun node_order = coda_node_order(doc : Doc, n : Node)
  fun node_order_weighted = coda_node_order_weighted(doc : Doc, n : Node, keys : UInt8**, weights : Float32*, count : LibC::SizeT)

  fun new_string = coda_new_string(doc : Doc, s : UInt8*, len : LibC::SizeT) : Node
  fun new_block = coda_new_block(doc : Doc) : Node
  fun new_array = coda_new_array(doc : Doc) : Node
  fun new_table = coda_new_table(doc : Doc) : Node
  fun new_keyed_table = coda_new_keyed_table(doc : Doc) : Node
  fun new_row = coda_new_row(doc : Doc) : Node
end
