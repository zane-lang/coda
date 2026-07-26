(* Foreign function declarations for Coda. The runtime value is an owning
   custom block with a finalizer; Obj.t keeps that representation private. *)

type native_doc = Obj.t

external coda_doc_new_ocaml : unit -> native_doc = "coda_doc_new_ocaml"
external coda_doc_free_ocaml : native_doc -> unit = "coda_doc_free_ocaml"
external coda_doc_parse_ocaml : string -> string option -> (native_doc * int * (int * int * int * int * string) option) = "coda_doc_parse_ocaml"
external coda_doc_parse_file_ocaml : string -> (native_doc * int * (int * int * int * int * string) option) = "coda_doc_parse_file_ocaml"
external coda_doc_serialize_ocaml : native_doc -> string option -> (string * int * (int * int * int * int * string) option) = "coda_doc_serialize_ocaml"
external coda_doc_root_ocaml : native_doc -> int = "coda_doc_root_ocaml"

external coda_node_kind_ocaml : native_doc -> int -> int = "coda_node_kind_ocaml"
external coda_node_is_container_ocaml : native_doc -> int -> bool = "coda_node_is_container_ocaml"
external coda_node_comment_get_ocaml : native_doc -> int -> string = "coda_node_comment_get_ocaml"
external coda_node_comment_set_ocaml : native_doc -> int -> string -> int = "coda_node_comment_set_ocaml"
external coda_node_header_comment_get_ocaml : native_doc -> int -> string = "coda_node_header_comment_get_ocaml"
external coda_node_header_comment_set_ocaml : native_doc -> int -> string -> int = "coda_node_header_comment_set_ocaml"

external coda_string_get_ocaml : native_doc -> int -> string = "coda_string_get_ocaml"
external coda_string_set_ocaml : native_doc -> int -> string -> int = "coda_string_set_ocaml"

external coda_array_len_ocaml : native_doc -> int -> int = "coda_array_len_ocaml"
external coda_array_get_ocaml : native_doc -> int -> int -> int = "coda_array_get_ocaml"
external coda_array_set_ocaml : native_doc -> int -> int -> int -> int = "coda_array_set_ocaml"
external coda_array_push_ocaml : native_doc -> int -> int -> int = "coda_array_push_ocaml"
external coda_array_remove_ocaml : native_doc -> int -> int -> int = "coda_array_remove_ocaml"

external coda_map_len_ocaml : native_doc -> int -> int = "coda_map_len_ocaml"
external coda_map_key_at_ocaml : native_doc -> int -> int -> string = "coda_map_key_at_ocaml"
external coda_map_value_at_ocaml : native_doc -> int -> int -> int = "coda_map_value_at_ocaml"
external coda_map_get_ocaml : native_doc -> int -> string -> int = "coda_map_get_ocaml"
external coda_map_get_or_insert_ocaml : native_doc -> int -> string -> int = "coda_map_get_or_insert_ocaml"
external coda_map_set_ocaml : native_doc -> int -> string -> int -> int = "coda_map_set_ocaml"
external coda_map_remove_ocaml : native_doc -> int -> string -> int = "coda_map_remove_ocaml"

external coda_table_col_count_ocaml : native_doc -> int -> int = "coda_table_col_count_ocaml"
external coda_table_col_name_ocaml : native_doc -> int -> int -> string = "coda_table_col_name_ocaml"
external coda_table_col_append_ocaml : native_doc -> int -> string -> int = "coda_table_col_append_ocaml"
external coda_table_row_count_ocaml : native_doc -> int -> int = "coda_table_row_count_ocaml"
external coda_table_row_at_ocaml : native_doc -> int -> int -> int = "coda_table_row_at_ocaml"
external coda_table_row_append_ocaml : native_doc -> int -> int -> int = "coda_table_row_append_ocaml"
external coda_table_row_set_ocaml : native_doc -> int -> int -> int -> int = "coda_table_row_set_ocaml"
external coda_table_row_remove_ocaml : native_doc -> int -> int -> int = "coda_table_row_remove_ocaml"

external coda_keyed_table_col_count_ocaml : native_doc -> int -> int = "coda_keyed_table_col_count_ocaml"
external coda_keyed_table_col_name_ocaml : native_doc -> int -> int -> string = "coda_keyed_table_col_name_ocaml"
external coda_keyed_table_col_append_ocaml : native_doc -> int -> string -> int = "coda_keyed_table_col_append_ocaml"
external coda_keyed_table_row_count_ocaml : native_doc -> int -> int = "coda_keyed_table_row_count_ocaml"
external coda_keyed_table_row_key_at_ocaml : native_doc -> int -> int -> string = "coda_keyed_table_row_key_at_ocaml"
external coda_keyed_table_row_at_ocaml : native_doc -> int -> int -> int = "coda_keyed_table_row_at_ocaml"
external coda_keyed_table_row_get_ocaml : native_doc -> int -> string -> int = "coda_keyed_table_row_get_ocaml"
external coda_keyed_table_row_set_ocaml : native_doc -> int -> string -> int -> int = "coda_keyed_table_row_set_ocaml"
external coda_keyed_table_row_remove_ocaml : native_doc -> int -> string -> int = "coda_keyed_table_row_remove_ocaml"

external coda_row_get_ocaml : native_doc -> int -> string -> string = "coda_row_get_ocaml"
external coda_row_set_ocaml : native_doc -> int -> string -> string -> int = "coda_row_set_ocaml"
external coda_row_remove_ocaml : native_doc -> int -> string -> int = "coda_row_remove_ocaml"
external coda_row_col_count_ocaml : native_doc -> int -> int = "coda_row_col_count_ocaml"
external coda_row_col_name_at_ocaml : native_doc -> int -> int -> string = "coda_row_col_name_at_ocaml"
external coda_row_col_value_at_ocaml : native_doc -> int -> int -> string = "coda_row_col_value_at_ocaml"
external coda_row_comment_get_ocaml : native_doc -> int -> string = "coda_row_comment_get_ocaml"
external coda_row_comment_set_ocaml : native_doc -> int -> string -> int = "coda_row_comment_set_ocaml"

external coda_node_serialize_ocaml : native_doc -> int -> string option -> (string * int * (int * int * int * int * string) option) = "coda_node_serialize_ocaml"
external coda_node_order_ocaml : native_doc -> int -> unit = "coda_node_order_ocaml"
external coda_node_order_weighted_ocaml : native_doc -> int -> (string * float) list -> unit = "coda_node_order_weighted_ocaml"

external coda_new_string_ocaml : native_doc -> string -> int = "coda_new_string_ocaml"
external coda_new_block_ocaml : native_doc -> int = "coda_new_block_ocaml"
external coda_new_array_ocaml : native_doc -> int = "coda_new_array_ocaml"
external coda_new_table_ocaml : native_doc -> int = "coda_new_table_ocaml"
external coda_new_keyed_table_ocaml : native_doc -> int = "coda_new_keyed_table_ocaml"
external coda_new_row_ocaml : native_doc -> int = "coda_new_row_ocaml"
