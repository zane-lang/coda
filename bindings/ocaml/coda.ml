open Coda_ocaml

type doc = native_doc
type node = int

let node_kind_of_int = function
  | 0 -> NodeNull
  | 2 -> NodeString
  | 3 -> NodeBlock
  | 4 -> NodeArray
  | 5 -> NodeTable
  | 6 -> NodeKeyedTable
  | 7 -> NodeRow
  | _ -> invalid_arg "Unknown node kind"

let status_of_int = function
  | 0 -> Ok
  | 1 -> Err
  | 2 -> NotFound
  | 3 -> BadKind
  | 4 -> OutOfRange
  | _ -> invalid_arg "Unknown status"

let parse_result (val_, status, err) =
  if status = 0 then Ok val_
  else
    match err with
    | () -> Error { code = status; line = 0; col = 0; offset = 0; message = "Unknown error" }
    | (code, line, col, offset, msg) -> 
        Error { code; line; col; offset; message = msg }

let opt_to_val = function
  | None -> ()
  | Some v -> v

let parse_file path = 
  parse_result (coda_doc_parse_file_ocaml path)

let parse src filename = 
  parse_result (coda_doc_parse_ocaml src (opt_to_val filename))

let serialize doc indent = 
  parse_result (coda_doc_serialize_ocaml doc (opt_to_val indent))

let serialize_node doc node indent = 
  parse_result (coda_node_serialize_ocaml doc node (opt_to_val indent))

let root doc = coda_doc_root_ocaml doc
let node_kind doc node = node_kind_of_int (coda_node_kind_ocaml doc node)
let node_is_container doc node = coda_node_is_container_ocaml doc node

let node_comment_get doc node = coda_node_comment_get_ocaml doc node
let node_comment_set doc node s = status_of_int (coda_node_comment_set_ocaml doc node s)

let node_header_comment_get doc node = coda_node_header_comment_get_ocaml doc node
let node_header_comment_set doc node s = status_of_int (coda_node_header_comment_set_ocaml doc node s)

let string_get doc node = coda_string_get_ocaml doc node
let string_set doc node s = status_of_int (coda_string_set_ocaml doc node s)

let array_len doc node = coda_array_len_ocaml doc node
let array_get doc node idx = coda_array_get_ocaml doc node idx
let array_set doc node idx value = status_of_int (coda_array_set_ocaml doc node idx value)
let array_push doc node value = status_of_int (coda_array_push_ocaml doc node value)
let array_remove doc node idx = status_of_int (coda_array_remove_ocaml doc node idx)

let map_len doc node = coda_map_len_ocaml doc node
let map_key_at doc node idx = coda_map_key_at_ocaml doc node idx
let map_value_at doc node idx = coda_map_value_at_ocaml doc node idx
let map_get doc node key = 
  let n = coda_map_get_ocaml doc node key in
  if n = 0 then None else Some n
let map_get_or_insert doc node key = coda_map_get_or_insert_ocaml doc node key
let map_set doc node key value = status_of_int (coda_map_set_ocaml doc node key value)
let map_remove doc node key = status_of_int (coda_map_remove_ocaml doc node key)

let table_col_count doc node = coda_table_col_count_ocaml doc node
let table_col_name doc node idx = coda_table_col_name_ocaml doc node idx
let table_col_append doc node name = status_of_int (coda_table_col_append_ocaml doc node name)

let table_row_count doc node = coda_table_row_count_ocaml doc node
let table_row_at doc node idx = coda_table_row_at_ocaml doc node idx
let table_row_append doc node row = status_of_int (coda_table_row_append_ocaml doc node row)
let table_row_set doc node idx row = status_of_int (coda_table_row_set_ocaml doc node idx row)
let table_row_remove doc node idx = status_of_int (coda_table_row_remove_ocaml doc node idx)

let keyed_table_col_count doc node = coda_keyed_table_col_count_ocaml doc node
let keyed_table_col_name doc node idx = coda_keyed_table_col_name_ocaml doc node idx
let keyed_table_col_append doc node name = status_of_int (coda_keyed_table_col_append_ocaml doc node name)

let keyed_table_row_count doc node = coda_keyed_table_row_count_ocaml doc node
let keyed_table_row_key_at doc node idx = coda_keyed_table_row_key_at_ocaml doc node idx
let keyed_table_row_at doc node idx = coda_keyed_table_row_at_ocaml doc node idx
let keyed_table_row_get doc node key = 
  let n = coda_keyed_table_row_get_ocaml doc node key in
  if n = 0 then None else Some n
let keyed_table_row_set doc node key row = status_of_int (coda_keyed_table_row_set_ocaml doc node key row)
let keyed_table_row_remove doc node key = status_of_int (coda_keyed_table_row_remove_ocaml doc node key)

let row_get doc node col = coda_row_get_ocaml doc node col
let row_set doc node col val_ = status_of_int (coda_row_set_ocaml doc node col val_)
let row_remove doc node col = status_of_int (coda_row_remove_ocaml doc node col)
let row_col_count doc node = coda_row_col_count_ocaml doc node
let row_col_name_at doc node idx = coda_row_col_name_at_ocaml doc node idx
let row_col_value_at doc node idx = coda_row_col_value_at_ocaml doc node idx
let row_comment_get doc node = coda_row_comment_get_ocaml doc node
let row_comment_set doc node s = status_of_int (coda_row_comment_set_ocaml doc node s)

let node_order doc node = coda_node_order_ocaml doc node

let new_string doc s = coda_new_string_ocaml doc s
let new_block doc = coda_new_block_ocaml doc
let new_array doc = coda_new_array_ocaml doc
let new_table doc = coda_new_table_ocaml doc
let new_keyed_table doc = coda_new_keyed_table_ocaml doc
let new_row doc = coda_new_row_ocaml doc
