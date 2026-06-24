#include "store/serialise.h"

#include <array>
#include <cstdint>
#include <string>

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

  store->ForEach([&](const std::string& key,
                     const std::optional<std::string>& value) {
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

}  // namespace myredis
