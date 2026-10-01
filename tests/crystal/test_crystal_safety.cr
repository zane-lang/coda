# The ownership and handle-validity guarantees of the Crystal binding, matching
# tests/python/test_python_safety.py.

require "../../bindings/crystal/coda"

def expect_error(message : String, &) : Nil
  begin
    yield
  rescue Coda::Error
    return
  end
  raise message
end

raise "parse_error_code_name" unless Coda.parse_error_code_name(2) == "DuplicateKey"

left, right = Coda::Doc.new, Coda::Doc.new
foreign = Coda::Block.new
left.root["foreign"] = foreign
expect_error("cross-document insertion succeeded") { right.root.insert("foreign", foreign) }
left.free
right.free

Coda::Doc.new do |doc|
  root = doc.root
  child = Coda::Block.new
  root["first"] = child
  expect_error("double-parent insertion succeeded") { root.insert("second", child) }
  expect_error("cycle insertion succeeded") { child.insert("self", child) }
end

Coda::Doc.parse("z last\na first\n") do |doc|
  z = doc.root["z"]
  doc.order
  raise "ordering invalidated a node" unless z.to_s == "last"
end

Coda::Doc.parse("table [\n z a\n]\n") do |doc|
  table = doc.root["table"].as_table
  raise "column order" unless table.columns == ["z", "a"]
  raise "serialized column order" unless doc.serialize.includes?("\n\tz a\n")
end

Coda::Doc.new do |doc|
  root = doc.root
  root["value"] = "old"
  stale = root["value"]
  root.delete("value")
  expect_error("stale node did not fail") { stale.to_s }
  root["value"] = "new"
  expect_error("stale node aliased a recycled node") { stale.to_s }
end

Coda::Doc.new do |doc|
  root = doc.root
  root["table"] = Coda::KeyedTable.new(["value"])
  table = root["table"].as_keyed_table
  {"z", "a"}.each { |key| table[key] = Coda::Row.new.insert("value", key) }
  table.order
  raise "keyed table order" unless table.keys == ["a", "z"]
  expect_error("column append invalidated existing rows") { table.append_col("later") }
  expect_error("attached row accepted an unknown field") { table["z"].insert("unknown", "x") }
  expect_error("attached row removed a required field") { table["z"].delete("value") }
end

Coda::Doc.parse("b 1\na 2\nc 3\n") do |doc|
  doc.order_weighted({"c" => 3, "b" => 2})
  raise "weights from a Hash" unless doc.root.keys == ["c", "b", "a"]
  doc.order_weighted([{"a", 9.0}].each)
  raise "weights from an Iterator" unless doc.root.keys.first == "a"
end

Coda::Doc.parse("list [\n x\n y\n]\n") do |doc|
  list = doc.root["list"].as_array
  raise "array []?" unless list[-1]?.to_s == "y" && list[2]?.nil? && list[-3]?.nil?
  raise "table from an Iterator" unless Coda::Table.new(["a", "b"].each).is_a?(Coda::Table)
end

doc = Coda::Doc.parse("name x\n")
node = doc.root["name"]
doc.free
expect_error("node outlived its freed document") { node.to_s }
expect_error("freed document was usable") { doc.root }
doc.free

puts "Crystal safety tests passed"
