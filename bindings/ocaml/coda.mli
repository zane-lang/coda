(* Thin OCaml bindings over the Coda C FFI.
   A [doc] owns node storage and is automatically finalized. Call [free] for
   deterministic release; using a document after [free] raises Failure.
   A [node] is an opaque integer handle, with [0] reserved as null/invalid. *)

type doc
type node = int

type node_kind =
  | NodeNull
  | NodeString
  | NodeBlock
  | NodeArray
  | NodeTable
  | NodeKeyedTable
  | NodeRow

type status =
  | Ok
  | Err
  | NotFound
  | BadKind
  | OutOfRange

type error = {
  code : int;
  line : int;
  col : int;
  offset : int;
  message : string;
}

val parse_file : string -> (doc, error) result
val parse : string -> string option -> (doc, error) result
val free : doc -> unit
val serialize : doc -> string option -> (string, error) result
val serialize_node : doc -> node -> string option -> (string, error) result

val root : doc -> node
val node_kind : doc -> node -> node_kind
val node_is_container : doc -> node -> bool

val node_comment_get : doc -> node -> string
val node_comment_set : doc -> node -> string -> status

val node_header_comment_get : doc -> node -> string
val node_header_comment_set : doc -> node -> string -> status

val string_get : doc -> node -> string
val string_set : doc -> node -> string -> status

val array_len : doc -> node -> int
val array_get : doc -> node -> int -> node
val array_set : doc -> node -> int -> node -> status
val array_push : doc -> node -> node -> status
val array_remove : doc -> node -> int -> status

val map_len : doc -> node -> int
val map_key_at : doc -> node -> int -> string
val map_value_at : doc -> node -> int -> node
val map_get : doc -> node -> string -> node option
val map_get_or_insert : doc -> node -> string -> node
val map_set : doc -> node -> string -> node -> status
val map_remove : doc -> node -> string -> status

val table_col_count : doc -> node -> int
val table_col_name : doc -> node -> int -> string
val table_col_append : doc -> node -> string -> status

val table_row_count : doc -> node -> int
val table_row_at : doc -> node -> int -> node
val table_row_append : doc -> node -> node -> status
val table_row_set : doc -> node -> int -> node -> status
val table_row_remove : doc -> node -> int -> status

val keyed_table_col_count : doc -> node -> int
val keyed_table_col_name : doc -> node -> int -> string
val keyed_table_col_append : doc -> node -> string -> status

val keyed_table_row_count : doc -> node -> int
val keyed_table_row_key_at : doc -> node -> int -> string
val keyed_table_row_at : doc -> node -> int -> node
val keyed_table_row_get : doc -> node -> string -> node option
val keyed_table_row_set : doc -> node -> string -> node -> status
val keyed_table_row_remove : doc -> node -> string -> status

val row_get : doc -> node -> string -> string
val row_set : doc -> node -> string -> string -> status
val row_remove : doc -> node -> string -> status
val row_col_count : doc -> node -> int
val row_col_name_at : doc -> node -> int -> string
val row_col_value_at : doc -> node -> int -> string
val row_comment_get : doc -> node -> string
val row_comment_set : doc -> node -> string -> status

val node_order : doc -> node -> unit
(* Reorder a sub-tree by weight (higher weight first; ties alphabetical),
   matching coda_node_order_weighted / the C++ Block::order semantics. *)
val node_order_weighted : doc -> node -> (string * float) list -> unit

val new_string : doc -> string -> node
val new_block : doc -> node
val new_array : doc -> node
val new_table : doc -> node
val new_keyed_table : doc -> node
val new_row : doc -> node
