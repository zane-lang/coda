# Coda Language Specification

Coda is a plain-text configuration language built around two structural primitives — **blocks** (named key–value sets) and **arrays** (ordered sequences that can also express flat tables). Values are untyped strings; structure is expressed purely through nesting. This document defines the full grammar, all validity constraints, and the serialisation rules.

---

## Lexical elements

### Strings and escapes

A **bare token** is any sequence of non-whitespace characters that does not include a syntax character (`{`, `}`, `[`, `]`, `"`, `#`). Bare tokens need no quoting.

```coda
url   https://example.com/path?query=1
email user@domain.com
date  2026-04-01
```

A **quoted string** is delimited by `"..."` and supports the following escape sequences:

| Escape | Character |
|--------|-----------|
| `\n`   | newline |
| `\t`   | tab |
| `\r`   | carriage return |
| `\"`   | double quote |
| `\\`   | backslash |

Unicode escapes (`\uXXXX`) are **not** supported. Any other `\X` sequence is a parse error.

```coda
message "hello\nworld"
path    "C:\\Users\\name"
quote   "He said \"hello\""
```

### Quoted keys

Keys follow the same rules as values — they can be bare tokens or quoted strings.

```coda
"key with spaces" value
"release manager" "Terry Davis"
```

### Reserved word: `key`

`key` is reserved when a key name is expected, and it is the marker token for a keyed-table header row (see [Keyed table](#keyed-table)). To use the literal string `key` as a block key, quote it. In value and table-cell positions, bare `key` is accepted as the string `key`; the serialiser writes it quoted so the output cannot be confused with a keyed-table header.

```coda
"key" value      # key named literally "key"
value key        # value is the string "key" (serialises as: value "key")
```

Using bare `key` where a block entry key is expected is a parse error.

### Comments

A `#` character begins a line comment; everything up to and including the newline is ignored for value purposes but **preserved** in the parse tree. Comments attach to the node that immediately follows them.

```coda
# Project configuration
name myproject

compiler {
	# Enable optimizations for release
	optimize true
	debug false
}
```

Comments also attach to individual array elements and table rows:

```coda
targets [
	# primary target
	x86_64-linux
	# secondary target
	x86_64-windows
]
```

#### Header comments on tables

A comment placed immediately before a plain-table or keyed-table header row cannot attach to any data node (the header row is not a value). It is stored as a **header comment** on the table container itself, separate from row comments.

```coda
points [
	# units: meters
	x y z
	1 2 3
]

deps [
	# optional deps
	key link version
	plot github.com/zane-lang/plot 4.0.3
]
```

Multiple consecutive comment lines before a header are joined with `\n` into a single string.

---

## Blocks `{}`

A block is an ordered map of named children. Every entry has an explicit string key. Keys must be unique within a block.

```coda
compiler {
	debug    false
	optimize true
}
```

Blocks nest freely — because every child is anchored by name, the hierarchy is always unambiguous:

```coda
project {
	name myproject
	compiler {
		debug false
	}
	targets [
		x86_64-linux
		x86_64-windows
		aarch64-macos
	]
}
```

**The opening `{` must be the last non-comment token on its line.** Any non-whitespace content after `{` on the same line is a parse error.

```coda
# Legal
compiler {
	debug false
}

# Illegal — content after opening brace
compiler { debug false }
```

The bare `key` token is not allowed as a block entry key. Quote it (`"key"`) for a literal key named `key`, or use `[]` for tabular data.

---

## Arrays `[]`

Arrays use `[...]`. The **mode** of an array is determined automatically from the first content line; there are four modes.

**The opening `[` must be the last non-comment token on its line.** Content begins on the next line.

### Bare list

When the first element is a single string token (not `{` or `[`), and each subsequent line also has exactly one token, the array is a **bare list** of strings.

```coda
targets [
	x86_64-linux
	x86_64-windows
	aarch64-macos
]
```

### Nested list

When the first element on the first content line is a block `{` or an array `[`, the array is a **nested list**. Each element is parsed as a full value — a block, a nested array, or a string.

```coda
rules [
	{
		name no-debug
		severity error
	}
	{
		name line-length
		severity warning
	}
]
```

Nested lists preserve the attachment of comments to individual elements.

### Plain table

When the first content line has **more than one token**, it is treated as the **header row**. Subsequent lines are data rows; each row must have exactly as many tokens as the header. All field names in the header must be unique.

```coda
points [
	x y z
	1 2 3
	4 5 6
]

changes [
	type desc
	feat "HolyC-style map ordering"
	fix  "comment attachment in tables"
]
```

Nesting (blocks or arrays) is not allowed inside plain-table rows.

### Keyed table

When the first token of the first content line is `key`, the array becomes a **keyed table** — an ordered map indexed by the first column of each row. The `key` token itself is consumed as the header marker and does not become a column name. All field names after `key` must be unique. All row keys must be unique.

```coda
deps [
	key  link                        version
	plot github.com/zane-lang/plot   4.0.3
	http github.com/zane-lang/http   2.1.0
]
```

Each data row has one more token than there are named fields (the extra token is the row key). Rows can be looked up by their key. Row iteration follows insertion order.

Nesting (blocks or arrays) is not allowed inside keyed-table rows.

---

## Nesting rules

**Nesting is allowed wherever every child has an explicit name or position anchor. Nesting is forbidden inside tabular rows, where a cell's meaning is determined solely by its column index.**

| Context | Nesting allowed |
|---------|-----------------|
| Block `{}` | ✅ |
| Bare list `[]` | ✅ |
| Nested list `[]` | ✅ |
| Plain table `[]` | ❌ |
| Keyed table `[ key … ]` | ❌ |

Placing a `{` or `[` inside a plain or keyed table row is a parse error.

**Legal:**
```coda
project {
	name myproject
	compiler {
		debug false
	}
}
```

**Illegal — block inside keyed-table row:**
```coda
deps [
	key  name   config
	http myhttp {
		timeout 30
	}
]
```

---

## Validity constraints

The following conditions are hard parse errors. Line and column information is included in every error.

| Error | Condition |
|-------|-----------|
| `UnexpectedToken` | A token appears where it is not grammatically valid (e.g. a bare `key` where a top-level key name is expected, a `]` with no opening `[`). |
| `UnexpectedEOF` | The file ends while a block `{}` or array `[]` is still open. |
| `DuplicateKey` | The same key appears more than once in a block, or the same row key appears more than once in a keyed table. |
| `DuplicateField` | The same field name appears more than once in a plain-table or keyed-table header row. |
| `RaggedRow` | A data row in a plain or keyed table has a different number of tokens than the header defines. |
| `InvalidEscape` | A quoted string contains a `\X` sequence where `X` is not one of `n`, `t`, `r`, `"`, `\`. |
| `UnterminatedString` | A quoted string is not closed before the end of the line or end of file. |
| `NestedBlock` | A `{` or `[` appears inside a plain-table or keyed-table row. |
| `ContentAfterBrace` | Non-whitespace content follows `{` or `[` on the same opening line. |
| `KeyInBlock` | The reserved word `key` appears where an entry key is expected inside an explicit block. |

Additional language-level invariants (not parse errors, but structural rules):

- A top-level document is an implicit block: a sequence of `key value` pairs, one per line.
- Blank lines between entries are allowed anywhere and are ignored.
- Keys and values at the same nesting level are separated by whitespace; no `=` or `:` delimiter is used.
- Every value is a string, block, bare list, nested list, plain table, or keyed table — there are no numeric, boolean, or null literals; consumers interpret strings as needed.

---

## Serialisation

### Quoting rules

When a value or key is written out, the serialiser applies the following rules to decide whether to quote it:

1. **Always quote** the empty string.
2. **Always quote** the reserved word `key` (written as `"key"`).
3. **Quote** any token that contains whitespace or any of the syntax characters `{`, `}`, `[`, `]`, `"`, `#`.
4. Otherwise write the token bare.

Inside a quoted string the serialiser escapes `\n`, `\t`, `\r`, `"`, and `\` using the sequences defined in [Strings and escapes](#strings-and-escapes).

### Round-trip stability

A parse–serialize–parse–serialize cycle is stable: the second serialised form is identical to the first. In particular:

- A token that needed no quotes when written is re-read as a bare token and written bare again.
- A token that required quotes is written with quotes, re-read as a quoted string, and written with quotes again.
- Comments (including header comments) are preserved through the cycle.

### `order()`

Calling `order()` on a block or keyed table reorders its entries in place, recursively, according to these rules:

1. Scalar string entries sort before container entries (blocks, arrays, tables).
2. Within each group, entries sort **alphabetically** by key.
3. The sort is applied recursively — nested blocks and keyed tables are also reordered.
4. Plain-table and array rows are **not** reordered (their order is part of their meaning).

A weighted variant accepts a function `weight(key) → float`. Entries with a **higher** weight sort earlier; entries with equal weight sort alphabetically. The recursive behaviour is otherwise the same.

```coda
# Before order()
compiler {
	optimize true
	debug    false
	name     clang
}

# After order()
compiler {
	debug    false
	name     clang
	optimize true
}
```
