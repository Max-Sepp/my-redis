#include "store/serialise.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace myredis {
namespace {

// Characters below this code point are control characters that JSON requires to
// be escaped.
constexpr uint8_t kFirstPrintable = 0x20;
constexpr uint8_t kNibbleMask = 0xf;

// Appends value as a JSON string literal (surrounded by quotes, with the
// characters required by RFC 8259 escaped) to out.
void AppendJsonString(const std::string& value, std::string& out) {
  out.push_back('"');
  for (const char character : value) {
    switch (character) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (static_cast<uint8_t>(character) < kFirstPrintable) {
          // Other control characters must be emitted as a \u00XX escape.
          constexpr std::array<char, 16> kHex = {'0', '1', '2', '3', '4', '5',
                                                 '6', '7', '8', '9', 'a', 'b',
                                                 'c', 'd', 'e', 'f'};
          out += "\\u00";
          out.push_back(kHex[(character >> 4) & kNibbleMask]);
          out.push_back(kHex[character & kNibbleMask]);
        } else {
          out.push_back(character);
        }
    }
  }
  out.push_back('"');
}

}  // namespace

std::string SerialiseMapToJson(
    const std::unique_ptr<Map<std::string, std::optional<std::string>>>&
        store) {
  std::string out = "{";
  bool first = true;

  store->ForEach(
      [&](const std::string& key, const std::optional<std::string>& value) {
        if (!first) out.push_back(',');
        first = false;

        AppendJsonString(key, out);
        out.push_back(':');
        if (value.has_value()) {
          AppendJsonString(*value, out);
        } else {
          out += "null";
        }
      });

  out.push_back('}');
  return out;
}

namespace {

// Skips JSON insignificant whitespace starting at pos.
void SkipWhitespace(const std::string& data, size_t& pos) {
  while (pos < data.size() && (data[pos] == ' ' || data[pos] == '\t' ||
                               data[pos] == '\n' || data[pos] == '\r')) {
    pos++;
  }
}

// The value a hex letter ('a'/'A') stands for.
constexpr int kHexLetterBase = 10;

// Returns the value of a hex digit, or -1 if chr is not one.
int HexValue(char chr) {
  if (chr >= '0' && chr <= '9') return chr - '0';
  if (chr >= 'a' && chr <= 'f') return chr - 'a' + kHexLetterBase;
  if (chr >= 'A' && chr <= 'F') return chr - 'A' + kHexLetterBase;
  return -1;
}

// Parses the JSON string literal at data[pos] (which must be the opening '"'),
// unescaping it and advancing pos past the closing quote. The inverse of
// AppendJsonString.
std::string ParseJsonString(const std::string& data, size_t& pos) {
  if (pos >= data.size() || data[pos] != '"') {
    throw std::invalid_argument("expected '\"' to open string");
  }
  pos++;  // consume opening quote

  std::string out;
  while (pos < data.size()) {
    const char character = data[pos++];
    if (character == '"') return out;  // closing quote
    if (character != '\\') {
      out.push_back(character);
      continue;
    }
    if (pos >= data.size()) throw std::invalid_argument("dangling escape");
    const char escape = data[pos++];
    switch (escape) {
      case '"': out.push_back('"'); break;
      case '\\': out.push_back('\\'); break;
      case '/': out.push_back('/'); break;
      case 'b': out.push_back('\b'); break;
      case 'f': out.push_back('\f'); break;
      case 'n': out.push_back('\n'); break;
      case 'r': out.push_back('\r'); break;
      case 't': out.push_back('\t'); break;
      case 'u': {
        if (pos + 4 > data.size()) {
          throw std::invalid_argument("truncated \\u escape");
        }
        int code = 0;
        for (int i = 0; i < 4; ++i) {
          const int nibble = HexValue(data[pos++]);
          if (nibble < 0) throw std::invalid_argument("invalid \\u escape");
          code = (code << 4) | nibble;
        }
        // SerialiseMapToJson only ever emits \u00XX (control bytes below
        // kFirstPrintable); those are the only escapes that map back to a
        // single byte.
        constexpr int kMaxByteValue = 0xff;
        if (code > kMaxByteValue) {
          throw std::invalid_argument("unsupported \\u escape");
        }
        out.push_back(static_cast<char>(code));
        break;
      }
      default:
        throw std::invalid_argument("invalid escape");
    }
  }
  throw std::invalid_argument("unterminated string");
}

}  // namespace

void DeserialiseJsonToMap(
    std::unique_ptr<Map<std::string, std::optional<std::string>>>& store,
    const std::string& json_data) {
  size_t pos = 0;
  SkipWhitespace(json_data, pos);
  if (pos >= json_data.size() || json_data[pos] != '{') {
    throw std::invalid_argument("json data did not begin with \"{\"");
  }
  pos++;

  SkipWhitespace(json_data, pos);
  if (pos < json_data.size() && json_data[pos] == '}') {
    pos++;  // empty object
  } else {
    while (true) {
      SkipWhitespace(json_data, pos);
      std::string key = ParseJsonString(json_data, pos);

      SkipWhitespace(json_data, pos);
      if (pos >= json_data.size() || json_data[pos] != ':') {
        throw std::invalid_argument("expected ':' after key");
      }
      pos++;

      SkipWhitespace(json_data, pos);
      std::optional<std::string> value;
      if (json_data.compare(pos, 4, "null") == 0) {
        pos += 4;
        value = std::nullopt;
      } else {
        value = ParseJsonString(json_data, pos);
      }
      store->Insert(std::move(key), std::move(value));

      SkipWhitespace(json_data, pos);
      if (pos >= json_data.size()) {
        throw std::invalid_argument("unterminated object");
      }
      if (json_data[pos] == ',') {
        pos++;
        continue;
      }
      if (json_data[pos] == '}') {
        pos++;
        break;
      }
      throw std::invalid_argument("expected ',' or '}'");
    }
  }

  SkipWhitespace(json_data, pos);
  if (pos != json_data.size()) {
    throw std::invalid_argument("trailing data after json object");
  }
}

}  // namespace myredis
