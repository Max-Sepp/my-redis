#ifndef MYREDIS_STORE_SERIALISE_H_
#define MYREDIS_STORE_SERIALISE_H_

#include <cstddef>
#include <cstdint>
#include <string>

namespace myredis {

// Appends value as a JSON string literal (surrounded by quotes, with the
// characters required by RFC 8259 escaped) to out.
void AppendJsonString(const std::string& value, std::string& out);

// Skips JSON insignificant whitespace starting at pos.
void SkipWhitespace(const std::string& data, size_t& pos);

// Parses the JSON string literal at data[pos] (which must be the opening
// '"'), unescaping it and advancing pos past the closing quote. The inverse
// of AppendJsonString. Throws std::invalid_argument on malformed input.
std::string ParseJsonString(const std::string& data, size_t& pos);

// Parses the JSON integer literal at data[pos] (an optional leading '-'
// followed by digits), advancing pos past it. Throws std::invalid_argument if
// there is no valid integer at pos.
int64_t ParseJsonInteger(const std::string& data, size_t& pos);

}  // namespace myredis

#endif  // MYREDIS_STORE_SERIALISE_H_
