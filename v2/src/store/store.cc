#include "store.h"

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include "store/map/standard_map.h"
#include "store/serialise.h"

namespace myredis {

Store::Store()
    : data_(std::make_unique<
            StandardMap<std::string, std::optional<std::string>>>()) {}

[[nodiscard]] std::optional<std::string> Store::Get(
    const std::string& key) const {
  const auto found = data_->LookUp(key);
  if (!found.has_value()) return std::nullopt;
  return found->get();
}

void Store::Set(std::string key, std::optional<std::string> value) {
  data_->Insert(std::move(key), std::move(value));
}

void Store::Del(const std::string& key) { data_->Remove(key); }

[[nodiscard]] std::string Store::SerialiseToJson() const {
  std::string out = "{";
  bool first = true;

  data_->ForEach(
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

void Store::DeserialiseFromJson(const std::string& json_data) {
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
      data_->Insert(std::move(key), std::move(value));

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
