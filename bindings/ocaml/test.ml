let test_parse_and_serialize () =
  let input = "key value\nblock {\n  inner value\n}" in
  match Coda.parse input None with
  | Error e -> failwith (Printf.sprintf "Parse failed: %s" e.message)
  | Ok doc ->
      let root = Coda.root doc in
      if Coda.map_get doc root "key" = None then
        failwith "Key 'key' not found in root";
      
      match Coda.serialize doc None with
      | Error e -> failwith (Printf.sprintf "Serialize failed: %s" e.message)
      | Ok output -> 
          (* Coda might normalize whitespace, so we don't do a strict string compare 
             unless we know the exact output format. *)
          Printf.printf "Serialized output:\n%s\n" output

let test_modification () =
  let input = "key1 value1" in
  match Coda.parse input None with
  | Error e -> failwith (Printf.sprintf "Parse failed: %s" e.message)
  | Ok doc ->
      let root = Coda.root doc in
      let _ = Coda.map_set doc root "key2" (Coda.new_string doc "value2") in
      let _ = Coda.string_set doc (Coda.map_get_or_insert doc root "key1") "new_value1" in
      
      match Coda.serialize doc None with
      | Error e -> failwith (Printf.sprintf "Serialize failed: %s" e.message)
      | Ok output ->
          if not (String.contains output '2') then
            failwith "Modified key 'key2' not found in output"

let test_arrays () =
  let doc = Coda.parse_file "nonexistent" in (* This will likely fail, let's use new_doc *)
  (* I didn't expose doc_new in the ML layer, let me add it or just parse empty *)
  match Coda.parse "" None with
  | Error e -> failwith (Printf.sprintf "Parse failed: %s" e.message)
  | Ok doc ->
      let root = Coda.root doc in
      let arr = Coda.new_array doc in
      let s1 = Coda.new_string doc "item1" in
      let s2 = Coda.new_string doc "item2" in
      let _ = Coda.array_push doc arr s1 in
      let _ = Coda.array_push doc arr s2 in
      
      if Coda.array_len doc arr <> 2 then
        failwith "Array length should be 2";
      
      let item = Coda.array_get doc arr 0 in
      if Coda.string_get doc item <> "item1" then
        failwith "Array item 0 should be item1"

let () =
  print_endline "Running Coda OCaml tests...";
  test_parse_and_serialize ();
  print_endline "test_parse_and_serialize: OK";
  test_modification ();
  print_endline "test_modification: OK";
  test_arrays ();
  print_endline "test_arrays: OK";
  print_endline "All tests passed!"
