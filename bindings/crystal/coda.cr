# Crystal bindings for the Coda configuration format.
#
# One class per AST node type, mirroring the C++ and Python APIs:
#
#     Block       ← { key value ... } — also used as the root node
#     Array       ← [ ... ]              homogeneous or nested values
#     Table       ← [ col1 col2 \n ... ] anonymous-row plain table
#     KeyedTable  ← [ key col1 col2 \n rowkey val1 val2 ] keyed table
#     Row         ← one row inside a Table or KeyedTable
#     StringNode  ← a leaf string value
#
# Usage:
#
#     Coda::Doc.parse(text) do |doc|
#       root = doc.root
#       name = root["name"].to_s                  # StringNode
#       root["block"].as_block.each { |key, value| }
#       root["table"].as_table.each { |row| puts row["col"] }
#       root["ktable"].as_keyed_table.each { |key, row| puts key, row["col"] }
#     end
#
# The checks the Python package layers on in `safety.py` (stale handles,
# cross-document attachment) are part of these classes.

require "./lib_coda"

module Coda
  # The C FFI ABI these bindings are written against.
  ABI_VERSION = 3_u32

  class Error < Exception
  end

  class ParseError < Error
    getter code : UInt32
    getter line : UInt32
    getter col : UInt32
    getter offset : UInt64

    def initialize(message : ::String, @code = 0_u32, @line = 0_u32, @col = 0_u32, @offset = 0_u64)
      super(message)
    end
  end

  # Any value that can be stored in a block or an array.
  alias Value = ::String | StringNode | Block | Array | Table | KeyedTable

  def self.abi_version : UInt32
    LibCoda.ffi_abi_version
  end

  # The name of a parse error code, such as `"DuplicateKey"`.
  def self.parse_error_code_name(code : Int) : ::String
    borrowed(LibCoda.parse_error_code_name(code.to_u32))
  end

  # :nodoc:
  def self.borrowed(s : LibCoda::Str) : ::String
    s.ptr.null? ? "" : ::String.new(s.ptr, s.len)
  end

  # :nodoc:
  # Copies the string out and releases the C buffer.
  def self.owned(s : LibCoda::OwnedStr) : ::String
    return "" if s.ptr.null?
    text = ::String.new(s.ptr, s.len)
    LibCoda.owned_str_free(s)
    text
  end

  # :nodoc:
  # The keys and weights as the C arrays `*_order_weighted` takes. The key
  # strings are returned too: the caller holds them until the call returns,
  # so the C strings stay alive whatever *pairs* is.
  #
  # *pairs* is read once, so an `Iterator` works too.
  def self.weights(pairs : Enumerable({::String, Number})) : {::Array(::String), ::Array(UInt8*), ::Array(Float32)}
    strings = [] of ::String
    values = [] of Float32
    pairs.each do |(key, weight)|
      strings << key
      values << weight.to_f32
    end
    {strings, strings.map(&.to_unsafe), values}
  end

  # :nodoc:
  # Wraps a node handle in the class for its kind.
  def self.wrap(doc : Doc, id : LibCoda::Node) : Node
    case LibCoda.node_kind(doc.to_unsafe, id)
    in .string?      then StringNode.wrap(doc, id)
    in .block?       then Block.wrap(doc, id)
    in .array?       then Array.wrap(doc, id)
    in .table?       then Table.wrap(doc, id)
    in .keyed_table? then KeyedTable.wrap(doc, id)
    in .row?         then Row.wrap(doc, id)
    in .null?        then raise Error.new("Node handle is stale or invalid")
    end
  end

  # :nodoc:
  def self.node(value : Value) : Node
    value.is_a?(::String) ? StringNode.new(value) : value
  end

  # :nodoc:
  def self.index(i : Int, size : Int, what : ::String) : Int32
    j = i < 0 ? i + size : i
    raise IndexError.new("#{what} index out of range") unless 0 <= j && j < size
    j.to_i32
  end

  # Base class for all node types.
  #
  # A node made with `new` is detached: it belongs to no document until it is
  # inserted, which places it in the parent's document. A detached node can be
  # inserted exactly once.
  abstract class Node
    @doc : Doc? = nil
    @id : LibCoda::Node = 0_u32

    # :nodoc:
    def self.wrap(doc : Doc, id : LibCoda::Node) : self
      node = allocate
      node.attach(doc, id)
      node
    end

    # :nodoc:
    protected def attach(@doc : Doc, @id : LibCoda::Node) : Nil
    end

    # :nodoc:
    def id : LibCoda::Node
      @id
    end

    # :nodoc:
    # The node's document, after checking the handle is still alive.
    def check : Doc
      doc = @doc
      raise Error.new("Node has not been attached to a document yet") unless doc
      doc.check
      if LibCoda.node_kind(doc.to_unsafe, @id).null?
        raise Error.new("Node handle is stale or invalid")
      end
      doc
    end

    # :nodoc:
    # Allocates a detached node in *doc*, or checks an attached one belongs to it.
    def materialize(doc : Doc) : Nil
      if own = @doc
        raise Error.new("Cannot attach a node from another document") unless own.same?(doc)
      else
        doc.check
        create(doc)
      end
    end

    # :nodoc:
    protected abstract def create(doc : Doc) : Nil

    # :nodoc:
    protected def created(doc : Doc, id : LibCoda::Node, what : ::String) : Nil
      raise Error.new("Failed to create #{what} node") if id == 0
      attach(doc, id)
    end

    def comment : ::String
      doc = check
      Coda.borrowed(LibCoda.node_comment_get(doc.to_unsafe, @id))
    end

    def comment=(value : ::String) : ::String
      doc = check
      status = LibCoda.node_comment_set(doc.to_unsafe, @id, value, value.bytesize)
      raise Error.new("Failed to set comment") unless status.ok?
      value
    end

    # True for a block, array, table or keyed table.
    def container? : Bool
      doc = check
      LibCoda.node_is_container(doc.to_unsafe, @id) != 0
    end

    # This node as Coda text.
    def serialize(indent : ::String = "\t") : ::String
      doc = check
      err = LibCoda::Error.new
      text = LibCoda.node_serialize(doc.to_unsafe, @id, indent, indent.bytesize, pointerof(err))
      if text.ptr.null?
        message = Coda.borrowed_error(pointerof(err))
        raise Error.new("Serialization failed: #{message}")
      end
      Coda.owned(text)
    end

    def as_string : StringNode
      as_kind(StringNode)
    end

    def as_block : Block
      as_kind(Block)
    end

    def as_array : Array
      as_kind(Array)
    end

    def as_table : Table
      as_kind(Table)
    end

    def as_keyed_table : KeyedTable
      as_kind(KeyedTable)
    end

    private def as_kind(type : T.class) : T forall T
      raise TypeCastError.new("Expected #{T.name.split("::").last}, got #{self.class.name.split("::").last}") unless is_a?(T)
      self.as(T)
    end

    def inspect(io : IO) : Nil
      io << self.class.name << "(node_id=" << @id << ')'
    end
  end

  # :nodoc:
  # Copies out the error's message and clears the error.
  def self.borrowed_error(err : LibCoda::Error*) : ::String
    message = err.value.message
    text = message.ptr.null? ? "" : ::String.new(message.ptr, message.len)
    LibCoda.error_clear(err)
    text
  end

  # A leaf string value.
  #
  # ```
  # s = Coda::StringNode.new("hello") # detached until inserted
  # ```
  class StringNode < Node
    def initialize(@pending : ::String = "")
    end

    protected def create(doc : Doc) : Nil
      created(doc, LibCoda.new_string(doc.to_unsafe, @pending, @pending.bytesize), "string")
    end

    def value : ::String
      doc = check
      Coda.borrowed(LibCoda.string_get(doc.to_unsafe, @id))
    end

    def value=(v : ::String) : ::String
      doc = check
      raise Error.new("Failed to set string value") unless LibCoda.string_set(doc.to_unsafe, @id, v, v.bytesize).ok?
      v
    end

    def to_s(io : IO) : Nil
      io << value
    end

    def ==(other : ::String) : Bool
      value == other
    end

    def ==(other : StringNode) : Bool
      value == other.value
    end

    def inspect(io : IO) : Nil
      io << "StringNode(" << value.inspect << ')'
    end
  end

  # One row of a `Table` or `KeyedTable`: a flat, ordered map of column name to
  # string.
  #
  # ```
  # row = Coda::Row.new
  # row["col1"] = "value"
  # ```
  #
  # A detached row takes any columns. Once attached, its columns are the
  # table's: values can change, but columns cannot be added or removed.
  class Row < Node
    include Enumerable({::String, ::String})

    def initialize
      @pending = {} of ::String => ::String
    end

    protected def create(doc : Doc) : Nil
      created(doc, LibCoda.new_row(doc.to_unsafe), "row")
      @pending.not_nil!.each do |col, val|
        LibCoda.row_set(doc.to_unsafe, @id, col, col.bytesize, val, val.bytesize)
      end
      @pending = nil
    end

    @pending : Hash(::String, ::String)? = nil

    # Rows have their own comment storage.
    def comment : ::String
      doc = check
      Coda.borrowed(LibCoda.row_comment_get(doc.to_unsafe, @id))
    end

    def comment=(value : ::String) : ::String
      doc = check
      raise Error.new("Failed to set row comment") unless LibCoda.row_comment_set(doc.to_unsafe, @id, value, value.bytesize).ok?
      value
    end

    # The value in *col*. Raises `KeyError` if the row has no such column.
    def [](col : ::String) : ::String
      self[col]? || raise KeyError.new("Missing column: #{col}")
    end

    def []?(col : ::String) : ::String?
      if pending = @pending
        return pending[col]?
      end
      doc = check
      # coda_row_get returns "" for a missing column, so look for it first.
      LibCoda.row_col_count(doc.to_unsafe, @id).times do |i|
        if Coda.borrowed(LibCoda.row_col_name_at(doc.to_unsafe, @id, i)) == col
          return Coda.borrowed(LibCoda.row_col_value_at(doc.to_unsafe, @id, i))
        end
      end
      nil
    end

    def []=(col : ::String, value : ::String) : ::String
      if pending = @pending
        return pending[col] = value
      end
      doc = check
      raise Error.new("Failed to set row field: #{col}") unless LibCoda.row_set(doc.to_unsafe, @id, col, col.bytesize, value, value.bytesize).ok?
      value
    end

    # Sets *col* and returns the row, for chaining.
    def insert(col : ::String, value : ::String) : self
      self[col] = value
      self
    end

    # Removes *col*. Raises `KeyError` if the row has no such column.
    def delete(col : ::String) : Nil
      if pending = @pending
        pending.delete(col) || raise KeyError.new("Missing column: #{col}")
        return
      end
      doc = check
      status = LibCoda.row_remove(doc.to_unsafe, @id, col, col.bytesize)
      raise KeyError.new("Missing column: #{col}") if status.not_found?
      raise Error.new("Failed to remove row field: #{col}") unless status.ok?
    end

    def has_key?(col : ::String) : Bool
      !self[col]?.nil?
    end

    # Yields each column name and value, in order.
    def each(& : {::String, ::String} ->) : Nil
      if pending = @pending
        pending.each { |pair| yield pair }
        return
      end
      doc = check
      LibCoda.row_col_count(doc.to_unsafe, @id).times do |i|
        yield({Coda.borrowed(LibCoda.row_col_name_at(doc.to_unsafe, @id, i)),
               Coda.borrowed(LibCoda.row_col_value_at(doc.to_unsafe, @id, i))})
      end
    end

    def size : Int32
      if pending = @pending
        return pending.size
      end
      doc = check
      LibCoda.row_col_count(doc.to_unsafe, @id).to_i32
    end

    def empty? : Bool
      size == 0
    end

    def to_h : Hash(::String, ::String)
      each_with_object({} of ::String => ::String) { |(col, val), h| h[col] = val }
    end
  end

  # A `{ key value ... }` block. The document root is a block.
  #
  # ```
  # block = Coda::Block.new
  # root["compiler"] = block
  # block["debug"] = "false"
  # ```
  class Block < Node
    include Enumerable({::String, Node})

    def initialize
    end

    protected def create(doc : Doc) : Nil
      created(doc, LibCoda.new_block(doc.to_unsafe), "block")
    end

    # Inserts or replaces the value under *key*, and returns the block for
    # chaining.
    def insert(key : ::String, value : Value) : self
      doc = check
      node = Coda.node(value)
      node.materialize(doc)
      raise Error.new("Failed to insert key: #{key}") unless LibCoda.map_set(doc.to_unsafe, @id, key, key.bytesize, node.id).ok?
      self
    end

    def []=(key : ::String, value : Value) : Value
      insert(key, value)
      value
    end

    # The node under *key*. Raises `KeyError` when it is absent; it never
    # inserts.
    def [](key : ::String) : Node
      self[key]? || raise KeyError.new("Missing key: #{key}")
    end

    def []?(key : ::String) : Node?
      doc = check
      child = LibCoda.map_get(doc.to_unsafe, @id, key, key.bytesize)
      child == 0 ? nil : Coda.wrap(doc, child)
    end

    def delete(key : ::String) : Nil
      doc = check
      status = LibCoda.map_remove(doc.to_unsafe, @id, key, key.bytesize)
      raise KeyError.new("Missing key: #{key}") if status.not_found?
      raise Error.new("Failed to remove key: #{key}") unless status.ok?
    end

    def has_key?(key : ::String) : Bool
      doc = check
      LibCoda.map_get(doc.to_unsafe, @id, key, key.bytesize) != 0
    end

    # The node under *key*, inserting an empty string node when it is absent.
    # This is the one lookup that inserts.
    def get_or_insert(key : ::String) : Node
      doc = check
      child = LibCoda.map_get_or_insert(doc.to_unsafe, @id, key, key.bytesize)
      raise Error.new("Failed to get or insert key: #{key}") if child == 0
      Coda.wrap(doc, child)
    end

    # Yields each key and node, in order.
    def each(& : {::String, Node} ->) : Nil
      doc = check
      LibCoda.map_len(doc.to_unsafe, @id).times do |i|
        key = Coda.borrowed(LibCoda.map_key_at(doc.to_unsafe, @id, i))
        yield({key, Coda.wrap(doc, LibCoda.map_value_at(doc.to_unsafe, @id, i))})
      end
    end

    def keys : ::Array(::String)
      map(&.[0])
    end

    def size : Int32
      doc = check
      LibCoda.map_len(doc.to_unsafe, @id).to_i32
    end

    def empty? : Bool
      size == 0
    end

    # Orders the fields: scalars before containers, alphabetical within each
    # group.
    def order : Nil
      doc = check
      LibCoda.node_order(doc.to_unsafe, @id)
    end

    # Orders the fields by descending weight, ties alphabetical.
    def order_weighted(weights : Enumerable({::String, Number})) : Nil
      doc = check
      strings, keys, values = Coda.weights(weights)
      return order if strings.empty?
      LibCoda.node_order_weighted(doc.to_unsafe, @id, keys.to_unsafe, values.to_unsafe, strings.size)
    end
  end

  # A `[ ... ]` array of ordered nodes.
  class Array < Node
    include Enumerable(Node)

    def initialize
    end

    protected def create(doc : Doc) : Nil
      created(doc, LibCoda.new_array(doc.to_unsafe), "array")
    end

    def header_comment : ::String
      doc = check
      Coda.borrowed(LibCoda.node_header_comment_get(doc.to_unsafe, @id))
    end

    def header_comment=(value : ::String) : ::String
      doc = check
      raise Error.new("Failed to set header_comment") unless LibCoda.node_header_comment_set(doc.to_unsafe, @id, value, value.bytesize).ok?
      value
    end

    # Appends *value* and returns the array, for chaining.
    def append(value : Value) : self
      doc = check
      node = Coda.node(value)
      node.materialize(doc)
      raise Error.new("Failed to append to array") unless LibCoda.array_push(doc.to_unsafe, @id, node.id).ok?
      self
    end

    def <<(value : Value) : self
      append(value)
    end

    # The element at *index*; negative indices count from the end. Raises
    # `IndexError` when out of range.
    def [](index : Int) : Node
      doc = check
      i = Coda.index(index, size, "array")
      child = LibCoda.array_get(doc.to_unsafe, @id, i)
      raise IndexError.new("array element missing") if child == 0
      Coda.wrap(doc, child)
    end

    def []?(index : Int) : Node?
      doc = check
      n = size
      i = index < 0 ? index + n : index
      return nil unless 0 <= i && i < n
      child = LibCoda.array_get(doc.to_unsafe, @id, i)
      child == 0 ? nil : Coda.wrap(doc, child)
    end

    def []=(index : Int, value : Value) : Value
      doc = check
      i = Coda.index(index, size, "array")
      node = Coda.node(value)
      node.materialize(doc)
      raise Error.new("Failed to set array element") unless LibCoda.array_set(doc.to_unsafe, @id, i, node.id).ok?
      value
    end

    def delete_at(index : Int) : Nil
      doc = check
      i = Coda.index(index, size, "array")
      raise Error.new("Failed to remove array element") unless LibCoda.array_remove(doc.to_unsafe, @id, i).ok?
    end

    def each(& : Node ->) : Nil
      doc = check
      LibCoda.array_len(doc.to_unsafe, @id).times do |i|
        child = LibCoda.array_get(doc.to_unsafe, @id, i)
        yield Coda.wrap(doc, child) unless child == 0
      end
    end

    def size : Int32
      doc = check
      LibCoda.array_len(doc.to_unsafe, @id).to_i32
    end

    def empty? : Bool
      size == 0
    end
  end

  # A plain table: rows in order, without keys.
  #
  # ```
  # root["releases"] = Coda::Table.new(["version", "date"])
  # releases = root["releases"].as_table
  # releases << Coda::Row.new.insert("version", "1.0.0").insert("date", "2026-01-01")
  # ```
  #
  # A row must hold exactly the table's columns when it is attached. Columns
  # can only be appended while the table has no rows.
  class Table < Node
    include Enumerable(Row)

    @pending : ::Array(::String)? = nil

    def initialize(columns : Enumerable(::String))
      pending = columns.to_a
      raise ArgumentError.new("Table requires at least one column") if pending.empty?
      @pending = pending
    end

    protected def create(doc : Doc) : Nil
      created(doc, LibCoda.new_table(doc.to_unsafe), "table")
      @pending.not_nil!.each { |col| append_col(col) }
      @pending = nil
    end

    def header_comment : ::String
      doc = check
      Coda.borrowed(LibCoda.node_header_comment_get(doc.to_unsafe, @id))
    end

    def header_comment=(value : ::String) : ::String
      doc = check
      raise Error.new("Failed to set header_comment") unless LibCoda.node_header_comment_set(doc.to_unsafe, @id, value, value.bytesize).ok?
      value
    end

    def columns : ::Array(::String)
      doc = check
      ::Array.new(LibCoda.table_col_count(doc.to_unsafe, @id).to_i) do |i|
        Coda.borrowed(LibCoda.table_col_name(doc.to_unsafe, @id, i))
      end
    end

    def append_col(name : ::String) : self
      doc = check
      raise Error.new("Failed to append column: #{name}") unless LibCoda.table_col_append(doc.to_unsafe, @id, name, name.bytesize).ok?
      self
    end

    # Appends *row* and returns the table, for chaining.
    def append(row : Row) : self
      doc = check
      row.materialize(doc)
      raise Error.new("Failed to append row") unless LibCoda.table_row_append(doc.to_unsafe, @id, row.id).ok?
      self
    end

    def <<(row : Row) : self
      append(row)
    end

    def [](index : Int) : Row
      doc = check
      i = Coda.index(index, size, "table row")
      row = LibCoda.table_row_at(doc.to_unsafe, @id, i)
      raise IndexError.new("table row missing") if row == 0
      Row.wrap(doc, row)
    end

    def []=(index : Int, row : Row) : Row
      doc = check
      i = Coda.index(index, size, "table row")
      row.materialize(doc)
      raise Error.new("Failed to set table row") unless LibCoda.table_row_set(doc.to_unsafe, @id, i, row.id).ok?
      row
    end

    def delete_at(index : Int) : Nil
      doc = check
      i = Coda.index(index, size, "table row")
      raise Error.new("Failed to remove table row") unless LibCoda.table_row_remove(doc.to_unsafe, @id, i).ok?
    end

    def each(& : Row ->) : Nil
      doc = check
      LibCoda.table_row_count(doc.to_unsafe, @id).times do |i|
        row = LibCoda.table_row_at(doc.to_unsafe, @id, i)
        yield Row.wrap(doc, row) unless row == 0
      end
    end

    def size : Int32
      doc = check
      LibCoda.table_row_count(doc.to_unsafe, @id).to_i32
    end

    def empty? : Bool
      size == 0
    end
  end

  # A keyed table: each row has a unique key, written as the first column.
  # `columns` lists the columns after the key.
  #
  # ```
  # root["deps"] = Coda::KeyedTable.new(["link", "version"])
  # deps = root["deps"].as_keyed_table
  # deps["plot"] = Coda::Row.new.insert("link", "github.com/zane-lang/plot").insert("version", "4.0.3")
  # deps["plot"]["link"]
  # ```
  class KeyedTable < Node
    include Enumerable({::String, Row})

    @pending : ::Array(::String)? = nil

    def initialize(columns : Enumerable(::String))
      pending = columns.to_a
      raise ArgumentError.new("KeyedTable requires at least one column") if pending.empty?
      @pending = pending
    end

    protected def create(doc : Doc) : Nil
      created(doc, LibCoda.new_keyed_table(doc.to_unsafe), "keyed table")
      @pending.not_nil!.each { |col| append_col(col) }
      @pending = nil
    end

    def header_comment : ::String
      doc = check
      Coda.borrowed(LibCoda.node_header_comment_get(doc.to_unsafe, @id))
    end

    def header_comment=(value : ::String) : ::String
      doc = check
      raise Error.new("Failed to set header_comment") unless LibCoda.node_header_comment_set(doc.to_unsafe, @id, value, value.bytesize).ok?
      value
    end

    def columns : ::Array(::String)
      doc = check
      ::Array.new(LibCoda.keyed_table_col_count(doc.to_unsafe, @id).to_i) do |i|
        Coda.borrowed(LibCoda.keyed_table_col_name(doc.to_unsafe, @id, i))
      end
    end

    def append_col(name : ::String) : self
      doc = check
      raise Error.new("Failed to append column: #{name}") unless LibCoda.keyed_table_col_append(doc.to_unsafe, @id, name, name.bytesize).ok?
      self
    end

    # Inserts or replaces the row under *key*, and returns the table for
    # chaining.
    def insert(key : ::String, row : Row) : self
      doc = check
      row.materialize(doc)
      raise Error.new("Failed to insert row: #{key}") unless LibCoda.keyed_table_row_set(doc.to_unsafe, @id, key, key.bytesize, row.id).ok?
      self
    end

    def []=(key : ::String, row : Row) : Row
      insert(key, row)
      row
    end

    # The row under *key*. Raises `KeyError` when it is absent; it never
    # inserts.
    def [](key : ::String) : Row
      self[key]? || raise KeyError.new("Missing row: #{key}")
    end

    def []?(key : ::String) : Row?
      doc = check
      row = LibCoda.keyed_table_row_get(doc.to_unsafe, @id, key, key.bytesize)
      row == 0 ? nil : Row.wrap(doc, row)
    end

    def delete(key : ::String) : Nil
      doc = check
      status = LibCoda.keyed_table_row_remove(doc.to_unsafe, @id, key, key.bytesize)
      raise KeyError.new("Missing row: #{key}") if status.not_found?
      raise Error.new("Failed to remove row: #{key}") unless status.ok?
    end

    def has_key?(key : ::String) : Bool
      doc = check
      LibCoda.keyed_table_row_get(doc.to_unsafe, @id, key, key.bytesize) != 0
    end

    # Yields each key and row, in order.
    def each(& : {::String, Row} ->) : Nil
      doc = check
      LibCoda.keyed_table_row_count(doc.to_unsafe, @id).times do |i|
        key = Coda.borrowed(LibCoda.keyed_table_row_key_at(doc.to_unsafe, @id, i))
        row = LibCoda.keyed_table_row_at(doc.to_unsafe, @id, i)
        yield({key, Row.wrap(doc, row)}) unless row == 0
      end
    end

    def keys : ::Array(::String)
      map(&.[0])
    end

    def size : Int32
      doc = check
      LibCoda.keyed_table_row_count(doc.to_unsafe, @id).to_i32
    end

    def empty? : Bool
      size == 0
    end

    # Orders the rows alphabetically by key.
    def order : Nil
      doc = check
      LibCoda.node_order(doc.to_unsafe, @id)
    end

    # Orders the rows by descending weight of their keys, ties alphabetical.
    def order_weighted(weights : Enumerable({::String, Number})) : Nil
      doc = check
      strings, keys, values = Coda.weights(weights)
      return order if strings.empty?
      LibCoda.node_order_weighted(doc.to_unsafe, @id, keys.to_unsafe, values.to_unsafe, strings.size)
    end
  end

  # A document. It owns every node in it; node objects become invalid when it
  # is freed.
  #
  # ```
  # Coda::Doc.parse(text) do |doc|
  #   doc.root["name"].to_s
  # end
  #
  # doc = Coda::Doc.new
  # doc.root["key"] = "value"
  # doc.save("out.coda")
  # doc.free
  # ```
  #
  # The block forms free the document when the block returns. Without them,
  # `free` releases it; the finalizer is only a backstop.
  class Doc
    @ptr : LibCoda::Doc

    # An empty document.
    def initialize
      @ptr = LibCoda.doc_new
      raise Error.new("Failed to create document") if @ptr.null?
    end

    protected def initialize(@ptr : LibCoda::Doc)
    end

    def self.new(& : Doc -> T) : T forall T
      use(new) { |doc| yield doc }
    end

    # Parses *text*. *filename* is used in error messages. Raises `ParseError`.
    def self.parse(text : ::String, filename : ::String? = nil) : Doc
      err = LibCoda::Error.new
      ptr = LibCoda.doc_parse(text, text.bytesize, filename.try(&.to_unsafe) || Pointer(UInt8).null, pointerof(err))
      raise parse_error(pointerof(err)) if ptr.null?
      new(ptr)
    end

    # Parses everything *io* holds.
    def self.parse(io : IO, filename : ::String? = nil) : Doc
      parse(io.gets_to_end, filename)
    end

    def self.parse(source : ::String | IO, filename : ::String? = nil, & : Doc -> T) : T forall T
      use(parse(source, filename)) { |doc| yield doc }
    end

    def self.parse_file(path : ::String | Path) : Doc
      err = LibCoda::Error.new
      ptr = LibCoda.doc_parse_file(path.to_s, pointerof(err))
      raise parse_error(pointerof(err)) if ptr.null?
      new(ptr)
    end

    def self.parse_file(path : ::String | Path, & : Doc -> T) : T forall T
      use(parse_file(path)) { |doc| yield doc }
    end

    private def self.use(doc : Doc, & : Doc -> T) : T forall T
      yield doc
    ensure
      doc.try &.free
    end

    private def self.parse_error(err : LibCoda::Error*) : ParseError
      e = err.value
      ParseError.new(Coda.borrowed_error(err), e.code, e.line, e.col, e.offset.to_u64)
    end

    # Releases the document. Safe to call more than once.
    def free : Nil
      unless @ptr.null?
        LibCoda.doc_free(@ptr)
        @ptr = LibCoda::Doc.null
      end
    end

    def freed? : Bool
      @ptr.null?
    end

    def finalize
      free
    end

    # :nodoc:
    def check : self
      raise Error.new("Doc has been freed") if @ptr.null?
      self
    end

    def to_unsafe : LibCoda::Doc
      @ptr
    end

    def root : Block
      check
      Block.wrap(self, LibCoda.doc_root(@ptr))
    end

    def serialize(indent : ::String = "\t") : ::String
      check
      err = LibCoda::Error.new
      text = LibCoda.doc_serialize(@ptr, indent, indent.bytesize, pointerof(err))
      if text.ptr.null?
        raise Error.new("Serialization failed: #{Coda.borrowed_error(pointerof(err))}")
      end
      Coda.owned(text)
    end

    def to_s(io : IO) : Nil
      io << serialize
    end

    def save(path : ::String | Path, indent : ::String = "\t") : Nil
      File.write(path, serialize(indent))
    end

    # Orders every block and keyed table in the document, as `Block#order`
    # and `KeyedTable#order` do.
    def order : Nil
      check
      LibCoda.doc_order(@ptr)
    end

    def order_weighted(weights : Enumerable({::String, Number})) : Nil
      check
      strings, keys, values = Coda.weights(weights)
      return order if strings.empty?
      LibCoda.doc_order_weighted(@ptr, keys.to_unsafe, values.to_unsafe, strings.size)
    end
  end

  if (abi = abi_version) != ABI_VERSION
    raise Error.new("Incompatible libcoda_ffi ABI: expected #{ABI_VERSION}, got #{abi}")
  end
end
