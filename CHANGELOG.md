# Changelog

## 3.0.0 — repository restructure & FFI fixes

### Fixed
- **FFI weighted ordering was inverted.** `coda_node_order_weighted` sorted
  lower weights to the top and ignored the alphabetical tie-break and the
  scalars-before-containers grouping. It now matches the C++/Python semantics
  exactly (higher weight first, ties alphabetical, scalars before containers).
- **FFI default node ordering** now uses a stable sort and groups scalars
  before containers (was an unstable, alphabetical-only sort).
- **Removed exceptions-as-control-flow** in the FFI `intern_value` (type
  dispatch now uses the AST variant `match`) and in the C++ test adapters.
- **`Doc::save(path, unit)`** no longer mutates the document's default indent;
  it is now a pure const operation.
- **Table / KeyedTable column order** is preserved consistently between empty
  and populated tables (empty tables no longer fall back to alphabetical).
- Honest, consistent handle-invalidation docs for the FFI order functions.

### Added
- **OCaml `node_order_weighted` binding** (C stub + externs + `.mli`), so the
  catalog's weighted-ordering test runs under OCaml too.

### Changed
- **Single test source of truth:** all four runners consume
  `tests/catalog/catalog.coda`.
- **C FFI test relocated to `tests/c/`** (separate from `tests/cpp/`): it is
  C++-written but exercises the C ABI, plus 6 FFI-only checks (ABI version,
  doc lifecycle, status codes, file parsing) that have no catalog equivalent.
- **OCaml runner coverage expanded** from a small subset to every catalog op
  that maps onto the binding surface (header_comment, map_keys, table_row_keys,
  plain_table_cell, array_block_field, *_comment, set_string[_path], ordering,
  …). The only remaining SKIPs are the `*_throws` ops, which assert exception
  behavior the OCaml (option/status) error model does not have.
- **tests/ restructured** into `harness/cpp`, `harness/python`, `cpp`,
  `python`, `ocaml`. Test code removed from `bindings/`.
- **OCaml tests now actually run** (`tests/ocaml/test_ocaml.ml`, catalog-driven,
  subset runner with explicit SKIP accounting). The old trivial smoke test in
  `bindings/ocaml/test.ml` was removed.
- `include/coda.hpp` is now a git-ignored generated artifact; recipes
  regenerate it on demand.
- Python: `requires-python` bumped to `>=3.11` (the bindings use `typing.Self`).
- Added `contributing/`, `.editorconfig`, this changelog.
- `devbox.json` `test` script now runs the real suite (`just test`).

### Removed (dead/orphaned code)
- `tests/harness/test_registry.hpp` (unused parallel framework)
- `tests/harness/test_data.hpp` (unused source-string fixtures)
- `src/helpers/macros.hpp` (unused `DEBUG` macro)
- `WrappingVariant` + the speculative `Archive` save/load in `helpers/types.hpp`
- `scripts/zig-cc` (never invoked)
- `bindings/ocaml/test.ml` (replaced by the real OCaml test runner)
