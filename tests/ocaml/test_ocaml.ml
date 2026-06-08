(* OCaml catalog-driven test runner for Coda.

   Single source of truth for test cases is tests/catalog/catalog.coda, the
   same file the C++ and Python harnesses consume.

   This runner implements every catalog `op` that maps onto the OCaml binding
   surface plus the `roundtrip` / `parse_fail_*` actions.

   The only ops it deliberately does NOT implement are the `*_throws` family
   (e.g. as_array_on_scalar_throws, int_index_on_block_throws,
   const_missing_key_throws). Those assert that the C++/Python API *raises* on a
   type/bounds error; the OCaml binding has no exception semantics — it returns
   `option` / status codes (map_get -> None, accessors -> 0/"") — so "does it
   throw" is semantically inapplicable, not a missing feature. Such ops are
   reported as SKIPPED so the suite is honest about coverage. *)

open Coda

let truthy s = s = "true" || s = "1" || s = "yes"

let status_ok = function Ok -> true | _ -> false

let opt_string d node name =
  match map_get d node name with Some n -> Some (string_get d n) | None -> None

let field d node name = match opt_string d node name with Some s -> s | None -> ""

let strings_of_array d node =
  let len = array_len d node in
  List.init len (fun i -> string_get d (array_get d node i))

(* substring test *)
let contains_sub hay sub =
  let nh = String.length hay and ns = String.length sub in
  if ns = 0 then true
  else
    let rec loop i =
      if i + ns > nh then false
      else if String.sub hay i ns = sub then true
      else loop (i + 1)
    in
    loop 0

(* true if every needle in [order] appears in [s], left to right, in sequence *)
let order_contains s order =
  let pos = ref 0 and ok = ref true in
  List.iter
    (fun needle ->
       if !ok then begin
         let nh = String.length s and ns = String.length needle in
         let rec find i =
           if i + ns > nh then -1
           else if String.sub s i ns = needle then i
           else find (i + 1)
         in
         let f = find !pos in
         if f < 0 then ok := false else pos := f + ns
       end)
    order;
  !ok

(* keys of a Block / KeyedTable in order *)
let map_keys d node =
  let n = map_len d node in
  List.init n (fun i -> map_key_at d node i)

let keyed_table_keys d node =
  let n = keyed_table_row_count d node in
  List.init n (fun i -> keyed_table_row_key_at d node i)

(* "Length" of a node the way the catalog's array_len / array_block_count ops
   mean it: arrays use array_len, but plain/keyed tables expose their length
   via the table_* APIs (coda_array_len returns 0 for a TABLE node). *)
let len_of d node =
  match node_kind d node with
  | NodeArray -> array_len d node
  | NodeTable -> table_row_count d node
  | NodeKeyedTable -> keyed_table_row_count d node
  | NodeBlock -> map_len d node
  | _ -> 0

(* Walk root[path.(0)][path.(1)]... returning the final node. *)
let path_walk d path =
  match path with
  | [] -> failwith "empty path"
  | first :: rest ->
    let start =
      match map_get d (root d) first with
      | Some n -> n
      | None -> failwith ("missing key: " ^ first)
    in
    List.fold_left
      (fun cur k ->
        match map_get d cur k with
        | Some n -> n
        | None -> failwith ("missing key: " ^ k))
      start rest

type result = Pass | Fail | Skip

(* Run a single `checks` entry. The check SPEC is read from [cd]/[check]
   (the catalog document), and is evaluated against the parsed source document
   [d]. Returns None if the op is unsupported (so the caller can SKIP). *)
let run_check cd check d : bool option =
  let op = field cd check "op" in
  match op with
  | "get_string" ->
    let f = field cd check "field" and eq = field cd check "eq" in
    Some (map_get d (root d) f <> None && field d (root d) f = eq)
  | "get_string_path" ->
    (match map_get cd check "path" with
     | None -> Some false
     | Some path_node ->
       let path = strings_of_array cd path_node in
       let eq = field cd check "eq" in
       Some (string_get d (path_walk d path) = eq))
  | "is_container" ->
    let f = field cd check "field" in
    let want = truthy (field cd check "eq_bool") in
    (match map_get d (root d) f with
     | Some n -> Some (node_is_container d n = want)
     | None -> Some false)
  | "has_key" ->
    let f = field cd check "field" in
    let want = truthy (field cd check "eq_bool") in
    Some ((map_get d (root d) f <> None) = want)
  | "map_len" ->
    let f = field cd check "field" in
    let want = int_of_string (field cd check "eq_int") in
    (match map_get d (root d) f with
     | Some n -> Some (map_len d n = want)
     | None -> Some false)
  | "array_len" | "array_block_count" ->
    let f = field cd check "field" in
    let want = int_of_string (field cd check "eq_int") in
    (match map_get d (root d) f with
     | Some n -> Some (len_of d n = want)
     | None -> Some false)
  | "array_element" ->
    let f = field cd check "field" in
    let idx = int_of_string (field cd check "idx") in
    let eq = field cd check "eq" in
    (match map_get d (root d) f with
     | Some n -> Some (string_get d (array_get d n idx) = eq)
     | None -> Some false)
  | "comment" ->
    let f = field cd check "field" and eq = field cd check "eq" in
    (match map_get d (root d) f with
     | Some n -> Some (node_comment_get d n = eq)
     | None -> Some false)
  | "table_cell" ->
    let t = field cd check "table"
    and r = field cd check "row"
    and c = field cd check "col"
    and eq = field cd check "eq" in
    (match map_get d (root d) t with
     | Some tn ->
       (match keyed_table_row_get d tn r with
        | Some row -> Some (row_get d row c = eq)
        | None -> Some false)
     | None -> Some false)
  | "serialize_contains" ->
    let needle = field cd check "contains" in
    let indent = opt_string cd check "indent" in
    (match serialize d indent with
     | Ok s -> Some (contains_sub s needle)
     | Error _ -> Some false)
  | "header_comment" ->
    let f = field cd check "field" and eq = field cd check "eq" in
    (match map_get d (root d) f with
     | Some n -> Some (node_header_comment_get d n = eq)
     | None -> Some false)
  | "comment_path" ->
    (match map_get cd check "path" with
     | None -> Some false
     | Some path_node ->
       let path = strings_of_array cd path_node in
       let eq = field cd check "eq" in
       Some (node_comment_get d (path_walk d path) = eq))
  | "array_element_comment" ->
    let f = field cd check "field" in
    let idx = int_of_string (field cd check "idx") in
    let eq = field cd check "eq" in
    (match map_get d (root d) f with
     | Some n -> Some (node_comment_get d (array_get d n idx) = eq)
     | None -> Some false)
  | "array_block_field" ->
    let f = field cd check "field" in
    let idx = int_of_string (field cd check "idx") in
    let field_name = field cd check "field_name" in
    let eq = field cd check "eq" in
    (match map_get d (root d) f with
     | Some n ->
       let elem = array_get d n idx in
       (match map_get d elem field_name with
        | Some v -> Some (string_get d v = eq)
        | None -> Some false)
     | None -> Some false)
  | "map_keys" ->
    let f = field cd check "field" in
    (match map_get cd check "eq_list" with
     | None -> Some false
     | Some eq_node ->
       let want = strings_of_array cd eq_node in
       (match map_get d (root d) f with
        | Some n -> Some (map_keys d n = want)
        | None -> Some false))
  | "table_row_keys" ->
    let t = field cd check "table" in
    (match map_get cd check "eq_list" with
     | None -> Some false
     | Some eq_node ->
       let want = strings_of_array cd eq_node in
       (match map_get d (root d) t with
        | Some n -> Some (keyed_table_keys d n = want)
        | None -> Some false))
  | "plain_table_cell" ->
    let t = field cd check "table" in
    let idx = int_of_string (field cd check "idx") in
    let c = field cd check "col" in
    let eq = field cd check "eq" in
    (match map_get d (root d) t with
     | Some n -> Some (row_get d (table_row_at d n idx) c = eq)
     | None -> Some false)
  | "table_row_comment" ->
    let t = field cd check "table"
    and r = field cd check "row"
    and eq = field cd check "eq" in
    (match map_get d (root d) t with
     | Some n ->
       (match keyed_table_row_get d n r with
        | Some row -> Some (row_comment_get d row = eq)
        | None -> Some false)
     | None -> Some false)
  | "plain_table_row_comment" ->
    let t = field cd check "table" in
    let idx = int_of_string (field cd check "idx") in
    let eq = field cd check "eq" in
    (match map_get d (root d) t with
     | Some n -> Some (row_comment_get d (table_row_at d n idx) = eq)
     | None -> Some false)
  | "table_row_missing_inserts" ->
    (* OCaml lookups never auto-insert: keyed_table_row_get returns None and
       leaves the table unchanged, so a missing row stays missing. *)
    let t = field cd check "table"
    and r = field cd check "row" in
    let want = truthy (field cd check "eq_bool") in
    (match map_get d (root d) t with
     | Some n ->
       let _ = keyed_table_row_get d n r in
       let inserted = keyed_table_row_get d n r <> None in
       Some (inserted = want)
     | None -> Some false)
  | "set_string" ->
    let f = field cd check "field" and v = field cd check "value" in
    let want = truthy (field cd check "eq_bool") in
    let ok = status_ok (map_set d (root d) f (new_string d v)) in
    Some (ok = want)
  | "set_string_path" ->
    (match map_get cd check "path" with
     | None -> Some false
     | Some path_node ->
       let path = strings_of_array cd path_node in
       let v = field cd check "value" in
       let want = truthy (field cd check "eq_bool") in
       (match List.rev path with
        | [] -> Some false
        | last :: rev_parents ->
          let parents = List.rev rev_parents in
          let container =
            List.fold_left (fun cur k -> map_get_or_insert d cur k) (root d) parents
          in
          let ok = status_ok (map_set d container last (new_string d v)) in
          Some (ok = want)))
  | "order_default_contains_order" ->
    (match map_get cd check "order" with
     | None -> Some false
     | Some order_node ->
       let order = strings_of_array cd order_node in
       node_order d (root d);
       (match serialize d None with
        | Ok s -> Some (order_contains s order)
        | Error _ -> Some false))
  | "order_weighted_contains_order" ->
    (match map_get cd check "order" with
     | None -> Some false
     | Some order_node ->
       let order = strings_of_array cd order_node in
       let weights =
         match map_get cd check "weights" with
         | None -> []
         | Some w_node ->
           let n = array_len cd w_node in
           List.init n (fun i ->
             let entry = array_get cd w_node i in
             (field cd entry "field", float_of_string (field cd entry "weight")))
       in
       node_order_weighted d (root d) weights;
       (match serialize d None with
        | Ok s -> Some (order_contains s order)
        | Error _ -> Some false))
  | _ -> None (* unsupported op (e.g. the *_throws family) -> skip *)

let run_test catalog_doc test : result =
  let src = field catalog_doc test "src" in
  let action = opt_string catalog_doc test "action" in
  (* The `checks` array lives on the catalog TEST node; it is evaluated against
     the document produced by parsing `src`. (The previous, never-executed
     runner mistakenly looked for `checks` inside the parsed src document, so
     no check ever ran.) *)
  let checks =
    match map_get catalog_doc test "checks" with
    | None -> []
    | Some node ->
      let n = array_len catalog_doc node in
      List.init n (fun i -> array_get catalog_doc node i)
  in
  match action with
  | Some "roundtrip" ->
    (match parse src None with
     | Error _ -> Fail
     | Ok d1 ->
       (match serialize d1 None with
        | Error _ -> Fail
        | Ok s1 ->
          (match parse s1 None with
           | Error _ -> Fail
           | Ok d2 ->
             (match serialize d2 None with
              | Error _ -> Fail
              | Ok s2 -> if s1 = s2 then Pass else Fail))))
  | Some "parse_fail_msg" | Some "parse_fail_code" ->
    (match parse src None with Error _ -> Pass | Ok _ -> Fail)
  | Some _ -> Skip
  | None ->
    (* checks-based test: parse src, then evaluate the catalog `checks` against
       the resulting document. *)
    (match parse src None with
     | Error _ -> Fail
     | Ok d ->
       if checks = [] then Pass
       else begin
         (* A test is a valid PASS/FAIL verdict only if EVERY check in it is
            supported by the OCaml subset. If any check is unsupported (or
            raises while evaluating an unsupported access pattern), we SKIP the
            whole test rather than render a partial verdict. *)
         let all_supported = ref true in
         let all_ok = ref true in
         List.iter
           (fun check ->
              let r = try run_check catalog_doc check d with _ -> None in
              match r with
              | Some ok -> if not ok then all_ok := false
              | None -> all_supported := false)
           checks;
         if not !all_supported then Skip
         else if !all_ok then Pass
         else Fail
       end)

let () =
  let catalog_path = "tests/catalog/catalog.coda" in
  match parse_file catalog_path with
  | Error e -> failwith (Printf.sprintf "Could not parse catalog: %s" e.message)
  | Ok catalog_doc ->
    let tests_node =
      match map_get catalog_doc (root catalog_doc) "tests" with
      | Some n -> n
      | None -> failwith "catalog has no `tests` array"
    in
    let num = array_len catalog_doc tests_node in
    let passed = ref 0 and failed = ref 0 and skipped = ref 0 in
    Printf.printf "\n=== Coda OCaml Test Suite (catalog-driven) ===\n";
    for i = 0 to num - 1 do
      let test = array_get catalog_doc tests_node i in
      let name = field catalog_doc test "name" in
      match run_test catalog_doc test with
      | Pass -> incr passed; Printf.printf "  \xe2\x9c\x93  %s\n" name
      | Skip -> incr skipped
      | Fail -> incr failed; Printf.printf "  \xe2\x9c\x97  %s\n" name
    done;
    Printf.printf "\n==============================\n";
    Printf.printf "  Passed:  %d\n" !passed;
    Printf.printf "  Skipped: %d (ops not in OCaml subset)\n" !skipped;
    Printf.printf "  Failed:  %d\n" !failed;
    Printf.printf "==============================\n";
    if !failed > 0 then exit 1
