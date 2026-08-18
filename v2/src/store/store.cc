#include "store.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include "store/map/standard_map.h"
#include "store/serialise.h"

namespace myredis {

Store::Store(std::unique_ptr<Time> time)
    : data_(std::make_unique<StandardMap<std::string, Entry>>()),
      time_(std::move(time)) {}

[[nodiscard]] std::optional<std::string> Store::Get(
    const std::string& key) const {
  const auto found = data_->LookUp(key);
  if (!found.has_value()) return std::nullopt;
  if (found->get().expiry_ != NO_EXPIRY &&
      found->get().expiry_ < time_->NowMs())
    return std::nullopt;
  return found->get().value_;
}

void Store::Set(std::string key, std::optional<std::string> value) {
  data_->Insert(std::move(key), Entry(std::move(value)));
}

void Store::Del(const std::string& key) { data_->Remove(key); }

bool Store::ExpireAt(const std::string& key, int64_t timestamp_ms) {
  return ExpireAt(key, timestamp_ms, ExpireOption::NA);
}

bool Store::ExpireAt(const std::string& key, int64_t timestamp_ms,
                      ExpireOption option) {
  const auto found = data_->LookUp(key);
  if (!found.has_value()) return false;
  Entry& entry = *found;

  switch (option) {
    case ExpireOption::NX:
      if (entry.expiry_ != NO_EXPIRY) return false;
      break;
    case ExpireOption::XX:
      if (entry.expiry_ == NO_EXPIRY) return false;
      break;
    case ExpireOption::GT:
      // A key with no expiry is treated as infinity, so GT never succeeds
      // against it.
      if (entry.expiry_ == NO_EXPIRY || entry.expiry_ >= timestamp_ms)
        return false;
      break;
    case ExpireOption::LT:
      // A key with no expiry is treated as infinity, so LT always succeeds
      // against it.
      if (entry.expiry_ != NO_EXPIRY && entry.expiry_ <= timestamp_ms)
        return false;
      break;
    case ExpireOption::NA:
      // NA means do nothing
      break;
  }

  entry.expiry_ = timestamp_ms;

  return true;
}

[[nodiscard]] std::int64_t Store::Ttl(const std::string& key) {
  const auto found = data_->LookUp(key);
  if (!found.has_value()) return -2;  // Key does not exist
  Entry& entry = *found;
  if (entry.expiry_ == NO_EXPIRY) return -1;  // Key exists but has no TTL
  const std::int64_t remaining_ms = entry.expiry_ - time_->NowMs();
  if (remaining_ms <= 0) return -2;  // Key has expired
  return remaining_ms;
}

[[nodiscard]] std::int64_t Store::NowMs() const { return time_->NowMs(); }

bool Store::Persist(const std::string& key) {
  const auto found = data_->LookUp(key);
  if (!found.has_value()) return false;
  Entry& entry = *found;
  if (entry.expiry_ == NO_EXPIRY) return false;
  if (entry.expiry_ < time_->NowMs()) return false;  // Key has expired
  entry.expiry_ = NO_EXPIRY;
  return true;
}

[[nodiscard]] std::string Store::SerialiseToJson() const {
  std::string out = "{";
  bool first = true;

  data_->ForEach([&](const std::string& key, const Entry& entry) {
    if (!first) out.push_back(',');
    first = false;

    AppendJsonString(key, out);
    out += ":{\"value\":";
    if (entry.value_.has_value()) {
      AppendJsonString(*entry.value_, out);
    } else {
      out += "null";
    }
    out += ",\"expiry\":";
    out += std::to_string(entry.expiry_);
    out.push_back('}');
  });

  out.push_back('}');
  return out;
}

Store::Entry Store::ParseEntryJson(const std::string& json_data, size_t& pos) {
  if (pos >= json_data.size() || json_data[pos] != '{') {
    throw std::invalid_argument("expected '{' to open entry");
  }
  pos++;

  SkipWhitespace(json_data, pos);
  if (ParseJsonString(json_data, pos) != "value") {
    throw std::invalid_argument("expected \"value\" field");
  }
  SkipWhitespace(json_data, pos);
  if (pos >= json_data.size() || json_data[pos] != ':') {
    throw std::invalid_argument("expected ':' after \"value\"");
  }
  pos++;

  SkipWhitespace(json_data, pos);
  std::optional<std::string> value;
  if (json_data.compare(pos, 4, "null") == 0) {
    pos += 4;
  } else {
    value = ParseJsonString(json_data, pos);
  }

  SkipWhitespace(json_data, pos);
  if (pos >= json_data.size() || json_data[pos] != ',') {
    throw std::invalid_argument("expected ',' after \"value\"");
  }
  pos++;

  SkipWhitespace(json_data, pos);
  if (ParseJsonString(json_data, pos) != "expiry") {
    throw std::invalid_argument("expected \"expiry\" field");
  }
  SkipWhitespace(json_data, pos);
  if (pos >= json_data.size() || json_data[pos] != ':') {
    throw std::invalid_argument("expected ':' after \"expiry\"");
  }
  pos++;

  SkipWhitespace(json_data, pos);
  const int64_t expiry = ParseJsonInteger(json_data, pos);

  SkipWhitespace(json_data, pos);
  if (pos >= json_data.size() || json_data[pos] != '}') {
    throw std::invalid_argument("expected '}' to close entry");
  }
  pos++;

  return {std::move(value), expiry};
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
      data_->Insert(std::move(key), ParseEntryJson(json_data, pos));

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
