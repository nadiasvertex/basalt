// Generates generated/basalt/tokens.h from Cubit DTCG token files.
//
// Usage: basalt_tokens <tokens_dir> <output_header>
//
// Aliases ("{a.b.c}") are resolved within one mode: primitives plus the
// semantic file of that mode. Color, dimension, string and typography tokens
// are emitted. `size.*` dimensions become `length`s whose unit (fixed or
// relative) follows the `size.fixed.*` / `size.relative.*` alias they resolve
// through. Each mode (light/dark, small/medium/large) is also emitted as an
// aggregate so themes can select one at run time.

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <print>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {

struct json_value;
using json_object = std::vector<std::pair<std::string, std::unique_ptr<json_value>>>;

struct json_value {
  std::variant<std::monostate, std::string, json_object> data;
};

class json_parser {
public:
  explicit json_parser(std::string_view text) : text_(text) {}

  [[nodiscard]] auto parse() -> std::expected<std::unique_ptr<json_value>, std::string> {
    auto value = parse_value();
    if (!value) {
      return std::unexpected(error_);
    }
    return value;
  }

private:
  auto fail(std::string message) -> std::unique_ptr<json_value> {
    error_ = std::move(message) + " at offset " + std::to_string(pos_);
    return nullptr;
  }

  void skip_space() {
    while (pos_ < text_.size() && std::string_view(" \t\r\n").contains(text_[pos_])) {
      ++pos_;
    }
  }

  auto parse_value() -> std::unique_ptr<json_value> {
    skip_space();
    if (pos_ >= text_.size()) {
      return fail("unexpected end");
    }
    char const c = text_[pos_];
    if (c == '{') {
      return parse_object();
    }
    if (c == '"') {
      auto s = parse_string();
      if (!s) {
        return nullptr;
      }
      auto v = std::make_unique<json_value>();
      v->data = std::move(*s);
      return v;
    }
    // Numbers, booleans and null are kept as their literal text.
    std::size_t const start = pos_;
    while (pos_ < text_.size() && !std::string_view(",}] \t\r\n").contains(text_[pos_])) {
      ++pos_;
    }
    auto v = std::make_unique<json_value>();
    v->data = std::string(text_.substr(start, pos_ - start));
    return v;
  }

  auto parse_string() -> std::optional<std::string> {
    std::string out;
    ++pos_;
    while (pos_ < text_.size() && text_[pos_] != '"') {
      if (text_[pos_] == '\\' && pos_ + 1 < text_.size()) {
        ++pos_;
        char const e = text_[pos_];
        out += e == 'n' ? '\n' : e == 't' ? '\t' : e;
      } else {
        out += text_[pos_];
      }
      ++pos_;
    }
    if (pos_ >= text_.size()) {
      error_ = "unterminated string";
      return std::nullopt;
    }
    ++pos_;
    return out;
  }

  auto parse_object() -> std::unique_ptr<json_value> {
    auto v = std::make_unique<json_value>();
    json_object members;
    ++pos_;
    skip_space();
    if (pos_ < text_.size() && text_[pos_] == '}') {
      ++pos_;
      v->data = std::move(members);
      return v;
    }
    while (true) {
      skip_space();
      if (pos_ >= text_.size() || text_[pos_] != '"') {
        return fail("expected key");
      }
      auto key = parse_string();
      if (!key) {
        return nullptr;
      }
      skip_space();
      if (pos_ >= text_.size() || text_[pos_] != ':') {
        return fail("expected ':'");
      }
      ++pos_;
      auto member = parse_value();
      if (!member) {
        return nullptr;
      }
      members.emplace_back(std::move(*key), std::move(member));
      skip_space();
      if (pos_ < text_.size() && text_[pos_] == ',') {
        ++pos_;
        continue;
      }
      if (pos_ < text_.size() && text_[pos_] == '}') {
        ++pos_;
        break;
      }
      return fail("expected ',' or '}'");
    }
    v->data = std::move(members);
    return v;
  }

  std::string_view text_;
  std::size_t pos_ = 0;
  std::string error_;
};

struct token {
  std::string type;
  std::string value;
  // Members of a composite value such as a typography token.
  std::map<std::string, std::string> fields;
};

using token_map = std::map<std::string, token>;

void flatten(json_value const &node, std::string const &path, token_map &out) {
  auto const *members = std::get_if<json_object>(&node.data);
  if (members == nullptr) {
    return;
  }
  token t;
  bool is_token = false;
  for (auto const &[key, child] : *members) {
    if (key == "$value") {
      if (auto const *s = std::get_if<std::string>(&child->data)) {
        t.value = *s;
      } else if (auto const *fields = std::get_if<json_object>(&child->data)) {
        for (auto const &[field, field_value] : *fields) {
          if (auto const *fs = std::get_if<std::string>(&field_value->data)) {
            t.fields[field] = *fs;
          }
        }
      }
      is_token = true;
    } else if (key == "$type") {
      if (auto const *s = std::get_if<std::string>(&child->data)) {
        t.type = *s;
      }
    }
  }
  if (is_token) {
    out[path] = std::move(t);
    return;
  }
  for (auto const &[key, child] : *members) {
    if (key.starts_with('$')) {
      continue;
    }
    flatten(*child, path.empty() ? key : path + "." + key, out);
  }
}

auto load_file(std::filesystem::path const &file, token_map &out)
    -> std::expected<void, std::string> {
  std::ifstream in(file);
  if (!in) {
    return std::unexpected("cannot open " + file.string());
  }
  std::stringstream buffer;
  buffer << in.rdbuf();
  auto parsed = json_parser(buffer.str()).parse();
  if (!parsed) {
    return std::unexpected(file.string() + ": " + parsed.error());
  }
  flatten(**parsed, "", out);
  return {};
}

auto alias_target(std::string const &value) -> std::optional<std::string> {
  if (value.size() > 2 && value.front() == '{' && value.back() == '}') {
    return value.substr(1, value.size() - 2);
  }
  return std::nullopt;
}

auto resolve(token_map const &all, std::string value) -> std::expected<std::string, std::string> {
  int depth = 0;
  while (auto key = alias_target(value)) {
    if (++depth > 32) {
      return std::unexpected("alias cycle at " + value);
    }
    auto const it = all.find(*key);
    if (it == all.end()) {
      return std::unexpected("unresolved alias " + value);
    }
    value = it->second.value;
  }
  return value;
}

auto is_size(std::string_view path) -> bool { return path.starts_with("size."); }

// The first fixed/relative primitive on the alias chain decides the unit;
// sizes that never pass through one are fixed.
auto size_unit(token_map const &all, std::string const &path) -> std::string_view {
  std::string key = path;
  for (int depth = 0; depth <= 32; ++depth) {
    if (key.starts_with("size.relative.")) {
      return "relative";
    }
    if (key.starts_with("size.fixed.")) {
      return "fixed";
    }
    auto const it = all.find(key);
    if (it == all.end()) {
      break;
    }
    auto next = alias_target(it->second.value);
    if (!next) {
      break;
    }
    key = *next;
  }
  return "fixed";
}

auto identifier(std::string const &path) -> std::string {
  std::string out;
  for (char const c : path) {
    out += (c == '.' || c == '-') ? '_' : c;
  }
  return out;
}

auto emit_color(std::string_view hex) -> std::expected<std::string, std::string> {
  if (hex.size() != 7 && hex.size() != 9) {
    return std::unexpected("bad color " + std::string(hex));
  }
  if (hex.front() != '#') {
    return std::unexpected("bad color " + std::string(hex));
  }
  auto byte = [&](std::size_t i) { return std::stoul(std::string(hex.substr(i, 2)), nullptr, 16); };
  auto const a = hex.size() == 9 ? byte(7) : 255UL;
  return std::format("{}, {}, {}, {}", byte(1), byte(3), byte(5), a);
}

auto emit_dimension(std::string_view text) -> std::string {
  std::string s(text);
  if (s.ends_with("px")) {
    s.resize(s.size() - 2);
  }
  if (s.find('.') == std::string::npos) {
    s += ".0";
  }
  return s + "f";
}

auto emit_string(std::string_view text) -> std::string {
  std::string out = "\"";
  for (char const c : text) {
    if (c == '"' || c == '\\') {
      out += '\\';
    }
    out += c;
  }
  return out + "\"";
}

auto emit_weight(std::string_view name) -> std::expected<std::string, std::string> {
  if (name == "Regular" || name == "400") {
    return "font_weight::regular";
  }
  if (name == "SemiBold" || name == "600") {
    return "font_weight::semibold";
  }
  if (name == "Bold" || name == "700") {
    return "font_weight::bold";
  }
  return std::unexpected("unsupported font weight " + std::string(name));
}

auto emit_typography(token_map const &all, std::string const &path, token const &t)
    -> std::expected<std::string, std::string> {
  auto field = [&](std::string const &name) -> std::expected<std::string, std::string> {
    auto const it = t.fields.find(name);
    if (it == t.fields.end()) {
      return std::unexpected(path + ": missing " + name);
    }
    return resolve(all, it->second);
  };
  for (auto const *ignored : {"textTransform", "textDecoration"}) {
    if (auto const it = t.fields.find(ignored); it != t.fields.end() && it->second != "none") {
      return std::unexpected(path + ": unsupported " + ignored + " " + it->second);
    }
  }
  auto family = field("fontFamily");
  auto size = field("fontSize");
  auto weight = field("fontWeight");
  auto spacing = field("letterSpacing");
  auto line_height = field("lineHeight");
  for (auto const *r : {&family, &size, &weight, &spacing, &line_height}) {
    if (!*r) {
      return std::unexpected(r->error());
    }
  }
  auto weight_name = emit_weight(*weight);
  if (!weight_name) {
    return std::unexpected(path + ": " + weight_name.error());
  }
  return std::format("{{.family = {}, .font_size = {}, .weight = {}, .letter_spacing = {}, "
                     ".line_height = {}}}",
                     emit_string(*family), emit_dimension(*size), *weight_name,
                     emit_dimension(*spacing), emit_dimension(*line_height));
}

auto is_emitted(token const &t) -> bool {
  return t.type == "color" || t.type == "dimension" || t.type == "string" || t.type == "typography";
}

// C++ type of the aggregate member that holds token `t` at `path`.
auto member_type(std::string const &path, token const &t) -> std::string_view {
  if (t.type == "color") {
    return "rgba";
  }
  if (t.type == "typography") {
    return "text_style";
  }
  return is_size(path) ? "length" : "float";
}

auto emit_namespace(std::string const &name, token_map const &all, token_map const &subset,
                    std::ostream &out) -> std::expected<void, std::string> {
  std::println(out, "namespace {} {{", name);
  for (auto const &[path, t] : subset) {
    auto const id = identifier(path);
    if (t.type == "typography") {
      auto style = emit_typography(all, path, t);
      if (!style) {
        return std::unexpected(style.error());
      }
      std::println(out, "inline constexpr text_style {}{};", id, *style);
      continue;
    }
    auto value = resolve(all, t.value);
    if (!value) {
      return std::unexpected(path + ": " + value.error());
    }
    if (t.type == "color") {
      auto c = emit_color(*value);
      if (!c) {
        return std::unexpected(path + ": " + c.error());
      }
      std::println(out, "inline constexpr rgba {}{{{}}};", id, *c);
    } else if (t.type == "dimension" && is_size(path)) {
      std::println(out, "inline constexpr length {} = {}({});", id, size_unit(all, path),
                   emit_dimension(*value));
    } else if (t.type == "dimension") {
      std::println(out, "inline constexpr float {} = {};", id, emit_dimension(*value));
    } else if (t.type == "string" && path.starts_with("type.weight.")) {
      auto weight = emit_weight(*value);
      if (!weight) {
        return std::unexpected(path + ": " + weight.error());
      }
      std::println(out, "inline constexpr font_weight {} = {};", id, *weight);
    } else if (t.type == "string") {
      std::println(out, "inline constexpr std::string_view {} = {};", id, emit_string(*value));
    }
  }
  std::println(out, "}}  // namespace {}\n", name);
  return {};
}

auto member_name(std::string const &path, std::string_view strip_prefix) -> std::string {
  auto id = identifier(path);
  return id.starts_with(strip_prefix) ? id.substr(strip_prefix.size()) : id;
}

// One aggregate type per token family, with members named after the token
// minus `strip_prefix`.
void emit_struct(std::string const &name, token_map const &subset, std::string_view strip_prefix,
                 std::ostream &out) {
  std::println(out, "struct {} {{", name);
  for (auto const &[path, t] : subset) {
    std::println(out, "  {} {};", member_type(path, t), member_name(path, strip_prefix));
  }
  std::println(
      out,
      "\n  [[nodiscard]] friend constexpr auto operator==({0} const &, {0} const &) -> bool "
      "= default;",
      name);
  std::println(out, "}};\n");
}

auto emit_instance(std::string const &type, std::string const &name, std::string const &ns,
                   token_map const &shape, token_map const &subset, std::string_view strip_prefix,
                   std::ostream &out) -> std::expected<void, std::string> {
  if (subset.size() != shape.size() ||
      !std::ranges::equal(subset | std::views::keys, shape | std::views::keys)) {
    return std::unexpected(ns + " does not define the same tokens as the other modes");
  }
  std::println(out, "inline constexpr {} {}{{", type, name);
  for (auto const &[path, t] : subset) {
    std::println(out, "    .{} = {}::{},", member_name(path, strip_prefix), ns, identifier(path));
  }
  std::println(out, "}};\n");
  return {};
}

auto subset_of(token_map const &m) -> token_map {
  token_map out;
  for (auto const &[k, v] : m) {
    if (is_emitted(v)) {
      out[k] = v;
    }
  }
  return out;
}

struct mode_family {
  std::string struct_name;
  std::string instance_suffix;
  std::string strip_prefix;
  std::vector<std::string> modes;
  std::vector<std::string> kinds;
};

auto run(std::filesystem::path const &dir, std::filesystem::path const &header)
    -> std::expected<void, std::string> {
  token_map base;
  for (auto const *file : {"color-primitive.default", "size-primitive.default", "opacity.default",
                           "type-primitive.default"}) {
    if (auto r = load_file(dir / std::format("{}.tokens.json", file), base); !r) {
      return r;
    }
  }

  std::ostringstream out;
  std::println(out,
               "// Generated by tools/basalt_tokens from the Cubit DTCG token files. Do not edit.");
  std::println(out, "#pragma once\n\n#include <string_view>\n\n#include \"basalt/color.h\"\n"
                    "#include \"basalt/typography.h\"\n#include \"basalt/units.h\"\n");
  std::println(out, "namespace basalt::tokens {{\n");

  if (auto r = emit_namespace("primitive", base, subset_of(base), out); !r) {
    return r;
  }

  std::vector<mode_family> const families{
      {"color_set", "_colors", "color_", {"light", "dark"}, {"color-semantic", "color-icon"}},
      {"size_set", "_sizes", "size_", {"small", "medium", "large"}, {"size-semantic"}},
  };
  for (auto const &family : families) {
    std::optional<token_map> shape;
    for (auto const &mode : family.modes) {
      token_map all = base;
      token_map semantic;
      for (auto const &kind : family.kinds) {
        if (auto r = load_file(dir / std::format("{}.{}.tokens.json", kind, mode), semantic); !r) {
          return r;
        }
      }
      semantic = subset_of(semantic);
      all.insert(semantic.begin(), semantic.end());
      if (!shape) {
        shape = semantic;
        emit_struct(family.struct_name, semantic, family.strip_prefix, out);
      }
      if (auto r = emit_namespace(mode, all, semantic, out); !r) {
        return r;
      }
      if (auto r = emit_instance(family.struct_name, mode + family.instance_suffix, mode, *shape,
                                 semantic, family.strip_prefix, out);
          !r) {
        return r;
      }
    }
  }

  token_map styles;
  if (auto r = load_file(dir / "text.styles.tokens.json", styles); !r) {
    return r;
  }
  styles = subset_of(styles);
  token_map all = base;
  all.insert(styles.begin(), styles.end());
  emit_struct("text_style_set", styles, "", out);
  if (auto r = emit_namespace("text", all, styles, out); !r) {
    return r;
  }
  if (auto r = emit_instance("text_style_set", "text_styles", "text", styles, styles, "", out);
      !r) {
    return r;
  }
  std::println(out, "}}  // namespace basalt::tokens");

  std::filesystem::create_directories(header.parent_path());
  std::ofstream file(header);
  if (!file) {
    return std::unexpected("cannot write " + header.string());
  }
  file << out.str();
  return {};
}

} // namespace

auto main(int argc, char **argv) -> int {
  if (argc != 3) {
    std::println(std::cerr, "usage: basalt_tokens <tokens_dir> <output_header>");
    return 2;
  }
  if (auto r = run(argv[1], argv[2]); !r) {
    std::println(std::cerr, "basalt_tokens: {}", r.error());
    return 1;
  }
  return 0;
}
