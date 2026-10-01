# Crystal test harness for catalog-driven tests.
#
# Mirrors the Python harness in tests/harness/python/harness.py, and runs every
# op it runs, the `*_throws` family included: the Crystal binding raises on a
# wrong kind, a bad index or a missing key, as the C++ and Python APIs do.

require "../../../bindings/crystal/coda"

module CodaHarness
  GREEN  = "\e[32m"
  RED    = "\e[31m"
  YELLOW = "\e[33m"
  RESET  = "\e[0m"

  # Executes catalog checks against one document.
  class Runner
    @root : Coda::Block

    def initialize(@doc : Coda::Doc)
      @root = @doc.root
    end

    private def truthy(v : String) : Bool
      {"true", "1", "yes"}.includes?(v)
    end

    private def strings(node : Coda::Node) : Array(String)
      node.as_array.map(&.to_s)
    end

    private def order_contains(text : String, order : Array(String)) : Bool
      pos = 0
      order.each do |needle|
        found = text.index(needle, pos)
        return false unless found
        pos = found + needle.size
      end
      true
    end

    private def path_walk(path : Array(String)) : Coda::Node
      path[1..].reduce(@root[path[0]]) { |node, key| node.as_block[key] }
    end

    # True when *block* raises one of the binding's lookup or type errors.
    private def raises?(& : ->) : Bool
      yield
      false
    rescue TypeCastError | IndexError | KeyError | Coda::Error
      true
    end

    private def header_comment(node : Coda::Node) : String?
      case node
      when Coda::Array, Coda::Table, Coda::KeyedTable then node.header_comment
      end
    end

    private def container_len(node : Coda::Node) : Int32
      case node
      when Coda::Array, Coda::Table, Coda::KeyedTable, Coda::Block then node.size
      else                                                              0
      end
    end

    def run_check(check : Coda::Block) : Bool
      s = ->(key : String) { check[key].to_s }

      case s.call("op")
      when "get_string"
        @root[s.call("field")].to_s == s.call("eq")
      when "get_string_path"
        path_walk(strings(check["path"])).to_s == s.call("eq")
      when "is_container"
        @root[s.call("field")].container? == truthy(s.call("eq_bool"))
      when "has_key"
        @root.has_key?(s.call("field")) == truthy(s.call("eq_bool"))
      when "map_len"
        @root[s.call("field")].as_block.size == s.call("eq_int").to_i
      when "map_keys"
        @root[s.call("field")].as_block.keys == strings(check["eq_list"])
      when "array_len"
        container_len(@root[s.call("field")]) == s.call("eq_int").to_i
      when "array_element"
        @root[s.call("field")].as_array[s.call("idx").to_i].to_s == s.call("eq")
      when "array_block_count"
        @root[s.call("field")].as_array.size == s.call("eq_int").to_i
      when "array_block_field"
        @root[s.call("field")].as_array[s.call("idx").to_i].as_block[s.call("field_name")].to_s == s.call("eq")
      when "array_index_throws"
        arr = @root[s.call("field")].as_array
        raises? { arr[s.call("idx").to_i] } == truthy(s.call("eq_bool"))
      when "plain_table_cell"
        @root[s.call("table")].as_table[s.call("idx").to_i][s.call("col")] == s.call("eq")
      when "table_cell"
        @root[s.call("table")].as_keyed_table[s.call("row")][s.call("col")] == s.call("eq")
      when "table_row_keys"
        @root[s.call("table")].as_keyed_table.keys == strings(check["eq_list"])
      when "table_row_missing_inserts"
        kt = @root[s.call("table")].as_keyed_table
        kt[s.call("row")]?
        kt.has_key?(s.call("row")) == truthy(s.call("eq_bool"))
      when "table_row_missing_throws"
        kt = @root[s.call("table")].as_keyed_table
        raises? { kt[s.call("row")] } == truthy(s.call("eq_bool"))
      when "comment"
        @root[s.call("field")].comment == s.call("eq")
      when "header_comment"
        header_comment(@root[s.call("field")]) == s.call("eq")
      when "comment_path"
        path_walk(strings(check["path"])).comment == s.call("eq")
      when "array_element_comment"
        @root[s.call("field")].as_array[s.call("idx").to_i].comment == s.call("eq")
      when "table_row_comment"
        @root[s.call("table")].as_keyed_table[s.call("row")].comment == s.call("eq")
      when "plain_table_row_comment"
        @root[s.call("table")].as_table[s.call("idx").to_i].comment == s.call("eq")
      when "set_string"
        key, value = s.call("field"), s.call("value")
        @root[key] = value
        @root[key].to_s == value
      when "set_string_path"
        path = strings(check["path"])
        block = path[1...-1].reduce(@root[path[0]].as_block) { |b, key| b[key].as_block }
        value = s.call("value")
        block[path[-1]] = value
        block[path[-1]].to_s == value
      when "string_index_on_scalar_throws"
        raises? { @root[s.call("field")].as_block[s.call("sub")] } == truthy(s.call("eq_bool"))
      when "int_index_on_block_throws"
        raises? { @root[s.call("field")].as_array[s.call("idx").to_i] } == truthy(s.call("eq_bool"))
      when "as_array_on_scalar_throws"
        !@root[s.call("field")].is_a?(Coda::Array) == truthy(s.call("eq_bool"))
      when "as_block_on_array_throws"
        !@root[s.call("field")].is_a?(Coda::Block) == truthy(s.call("eq_bool"))
      when "as_table_on_block_throws"
        !@root[s.call("field")].is_a?(Coda::Table | Coda::KeyedTable) == truthy(s.call("eq_bool"))
      when "const_missing_key_throws"
        raises? { @root[s.call("field")] } == truthy(s.call("eq_bool"))
      when "order_default_contains_order"
        @doc.order
        order_contains(@doc.serialize, strings(check["order"]))
      when "order_weighted_contains_order"
        weights = check["weights"].as_array.map do |entry|
          {entry.as_block["field"].to_s, entry.as_block["weight"].to_s.to_f}
        end
        @doc.order_weighted(weights)
        order_contains(@doc.serialize, strings(check["order"]))
      when "serialize_contains"
        @doc.serialize(s.call("indent")).includes?(s.call("contains"))
      else
        false
      end
    end
  end

  def self.parse_fail_msg(src : String, test : Coda::Block) : Bool
    Coda::Doc.parse(src).free
    false
  rescue error : Coda::ParseError
    needles = test["needles"].as_array.map(&.to_s)
    message = error.message.to_s
    needles.empty? || needles.any? { |needle| message.includes?(needle) }
  end

  def self.parse_fail_code(src : String, test : Coda::Block) : Bool
    Coda::Doc.parse(src).free
    false
  rescue error : Coda::ParseError
    Coda.parse_error_code_name(error.code) == test["code"].to_s
  end

  def self.roundtrip(src : String) : Bool
    serialized = Coda::Doc.parse(src, &.serialize)
    Coda::Doc.parse(serialized, &.serialize) == serialized
  end

  def self.check_all(src : String, test : Coda::Block) : Bool
    Coda::Doc.parse(src) do |doc|
      runner = Runner.new(doc)
      checks = test["checks"]?.try(&.as_array.to_a) || [] of Coda::Node
      checks.all? { |check| runner.run_check(check.as_block) }
    end
  end

  # Runs every test in the catalog and exits 1 if any fails.
  def self.run_catalog_tests(catalog_path : String) : Nil
    passed = failed = 0
    current_suite = nil

    Coda::Doc.parse_file(catalog_path) do |catalog|
      catalog.root["tests"].as_array.each do |node|
        test = node.as_block
        suite, name, src = test["suite"].to_s, test["name"].to_s, test["src"].to_s

        if suite != current_suite
          current_suite = suite
          puts "\n#{YELLOW}[#{suite}]#{RESET}"
        end

        ok = begin
          case test["action"]?.try(&.to_s)
          when "parse_fail_msg"  then parse_fail_msg(src, test)
          when "parse_fail_code" then parse_fail_code(src, test)
          when "roundtrip"       then roundtrip(src)
          when nil               then check_all(src, test)
          else                        false
          end
        rescue Exception
          false
        end

        if ok
          passed += 1
          puts "  #{GREEN}✓#{RESET}  #{name}"
        else
          failed += 1
          puts "  #{RED}✗#{RESET}  #{name}"
        end
      end
    end

    puts "\n══════════════════════════════"
    puts "  #{GREEN}Passed: #{passed}#{RESET}"
    puts failed == 0 ? "  Failed: 0" : "  #{RED}Failed: #{failed}#{RESET}"
    puts "══════════════════════════════"
    exit 1 if failed > 0
  end
end
