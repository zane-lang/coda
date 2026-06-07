open Coda

module TestRunner = struct
  type t = { doc : doc; root : node }

  let create d = { doc = d; root = root d }

  let bool_of_string s = s = "true" || s = "1" || s = "yes"
  let int_of_string s = int_of_string s
  let float_of_string s = float_of_string s

  let strings_of_node d node =
    let len = array_len d node in
    Array.init len (fun i -> string_get d (array_get d node i))

  let path_walk d start_key keys =
    let node = match map_get d (root d) start_key with Some n -> n | None -> failwith "Key not found" in
    List.fold_left (fun curr_node key -> 
      match map_get d curr_node key with
      | None -> failwith "Key not found"
      | Some n -> n
    ) node keys

  let run_check d check =
    let op = match map_get d check "op" with 
             | Some n -> string_get d n
             | None -> failwith "No op field"
    in
    match op with
    | "get_string" ->
        let field = match map_get d check "field" with Some n -> string_get d n | _ -> "" in
        let eq = match map_get d check "eq" with Some n -> string_get d n | _ -> "" in
        let got = match map_get d (root d) field with Some n -> string_get d n | _ -> "" in
        got = eq

    | "get_string_path" ->
        let path_node = match map_get d check "path" with Some n -> n | _ -> 0 in
        let path = strings_of_node d path_node in
        let eq = match map_get d check "eq" with Some n -> string_get d n | _ -> "" in
        let got_node = path_walk d (List.hd path) (List.tl path) in
        string_get d got_node = eq

    | "is_container" ->
        let field = match map_get d check "field" with Some n -> string_get d n | _ -> "" in
        let eq_bool = match map_get d check "eq_bool" with Some n -> bool_of_string (string_get d n) | _ -> false in
        let got_node = match map_get d (root d) field with Some n -> n | _ -> 0 in
        node_is_container d got_node = eq_bool

    | "has_key" ->
        let field = match map_get d check "field" with Some n -> string_get d n | _ -> "" in
        let eq_bool = match map_get d check "eq_bool" with Some n -> bool_of_string (string_get d n) | _ -> false in
        let got = map_get d (root d) field <> None in
        got = eq_bool

    | "map_len" ->
        let field = match map_get d check "field" with Some n -> string_get d n | _ -> "" in
        let eq_int = match map_get d check "eq_int" with Some n -> int_of_string (string_get d n) | _ -> 0 in
        let got_node = match map_get d (root d) field with Some n -> n | _ -> 0 in
        map_len d got_node = eq_int

    | "array_len" ->
        let field = match map_get d check "field" with Some n -> string_get d n | _ -> "" in
        let eq_int = match map_get d check "eq_int" with Some n -> int_of_string (string_get d n) | _ -> 0 in
        let got_node = match map_get d (root d) field with Some n -> n | _ -> 0 in
        array_len d got_node = eq_int

    | "comment" ->
        let field = match map_get d check "field" with Some n -> string_get d n | _ -> "" in
        let eq = match map_get d check "eq" with Some n -> string_get d n | _ -> "" in
        let got_node = match map_get d (root d) field with Some n -> n | _ -> 0 in
        node_comment_get d got_node = eq

    | "serialize_contains" ->
        let contains = match map_get d check "contains" with Some n -> string_get d n | _ -> "" in
        let indent = match map_get d check "indent" with Some n -> Some (string_get d n) | _ -> None in
        match serialize d indent with
        | Ok s -> String.contains s contains (* Simplified check *)
        | Error _ -> false

    | _ -> 
        Printf.printf "Unsupported op: %s\n" op;
        false
end

let run_catalog_tests catalog_path =
  match parse_file catalog_path with
  | Error e -> failwith (Printf.sprintf "Could not parse catalog: %s" e.message)
  | Ok catalog_doc ->
      let root = Coda.root catalog_doc in
      let tests_node = match map_get catalog_doc root "tests" with Some n -> n | None -> failwith "No tests in catalog" in
      let num_tests = array_len catalog_doc tests_node in
      let passed = ref 0 in
      let failed = ref 0 in
      
      for i = 0 to num_tests - 1 do
        let test_node = array_get catalog_doc tests_node i in
        let name = match map_get catalog_doc test_node "name" with Some n -> string_get catalog_doc n | _ -> "unnamed" in
        let src = match map_get catalog_doc test_node "src" with Some n -> string_get catalog_doc n | _ -> "" in
        
        let ok = match parse catalog_doc src None with
          | Error _ -> false
          | Ok doc ->
              let runner = TestRunner.create doc in
              let checks_node = match map_get doc (Coda.root doc) "checks" with Some n -> n | None -> 0 in
              if checks_node = 0 then true
              else
                let num_checks = array_len doc checks_node in
                let all_ok = ref true in
                for j = 0 to num_checks - 1 do
                  let check = array_get doc checks_node j in
                  if not (TestRunner.run_check doc check) then all_ok := false
                done;
                !all_ok
        in
        if ok then (incr passed; Printf.printf "✓ %s\n" name)
        else (incr failed; Printf.printf "✗ %s\n" name)
      done;
      Printf.printf "\nPassed: %d, Failed: %d\n" !passed !failed;
      if !failed > 0 then exit 1

let () =
  let catalog_path = "tests/catalog/catalog.coda" in
  run_catalog_tests catalog_path
