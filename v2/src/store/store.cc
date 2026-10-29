#include "store.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

#include "data_structures/map/hash.h"
#include "data_structures/map/linear_probing_hashmap.h"
#include "store/serialise.h"
#include "time/time.h"

namespace myredis {

Store::Store(std::unique_ptr<Time> time, const std::uint64_t rng_seed)
    : data_(std::make_unique<LinearProbingHashmap<std::string, Entry>>(
          kDefaultLoadFactor, StringHash)),
      expires_(
          std::make_unique<LinearProbingHashmap<std::string, std::monostate>>(
              kDefaultLoadFactor, StringHash)),
      time_(std::move(time)),
      rng_(rng_seed) {}

[[nodiscard]] std::optional<std::string> Store::Get(
    const std::string& key) const {
  const auto found = data_->LookUp(key);
  if (!found.has_value()) return std::nullopt;
  if (IsExpired(found->get(), time_->NowMs())) return std::nullopt;
  return found->get().value_;
}

void Store::Set(std::string key, std::optional<std::string> value) {
  // Overwriting a key discards its TTL
  expires_->Remove(key);
  data_->Insert(std::move(key), Entry(std::move(value)));
}

void Store::Del(const std::string& key) {
  data_->Remove(key);
  expires_->Remove(key);
}

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
  expires_->Insert(key, std::monostate{});

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
  expires_->Remove(key);
  return true;
}

std::size_t Store::ActiveExpireCycle(
    const ExpireContinuePredicate& should_continue) {
  const std::int64_t now_ms = time_->NowMs();
  std::size_t total_removed = 0;
  bool keep_sampling = true;
  while (keep_sampling) {
    // Sampling is with replacement, so a small index may yield the same live
    // key more than once.
    ExpireRound round{.sampled = 0, .removed = 0};
    bool index_empty = false;
    while (!index_empty && round.sampled < kExpireSampleSize) {
      const std::optional<std::string> key = expires_->RandomKey(rng_);
      if (!key.has_value()) {
        index_empty = true;
      } else {
        round.sampled++;
        if (RemoveIfExpired(*key, now_ms)) round.removed++;
      }
    }
    total_removed += round.removed;
    keep_sampling = !index_empty && should_continue(round);
  }
  return total_removed;
}

std::size_t Store::ActiveExpireCycle() {
  const std::int64_t start_ms = time_->NowMs();
  // A round that was mostly live suggests few expired keys remain, so stop
  // until the next cycle.
  return ActiveExpireCycle([this, start_ms](const ExpireRound& round) {
    return round.removed * kExpireRepeatRatio > round.sampled &&
           time_->NowMs() - start_ms < kExpireCycleBudgetMs;
  });
}

bool Store::RemoveIfExpired(const std::string& key, const int64_t now_ms) {
  const auto found = data_->LookUp(key);
  if (found.has_value() && !IsExpired(found->get(), now_ms)) return false;
  // A missing key would mean the index fell out of sync; drop it either way.
  data_->Remove(key);
  expires_->Remove(key);
  return found.has_value();
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
      Entry entry = ParseEntryJson(json_data, pos);
      if (entry.expiry_ != NO_EXPIRY) expires_->Insert(key, std::monostate{});
      data_->Insert(std::move(key), std::move(entry));

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
