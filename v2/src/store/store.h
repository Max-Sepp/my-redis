#ifndef MYREDIS_STORE_STORE_H_
#define MYREDIS_STORE_STORE_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "store/map/map.h"
#include "time/time.h"

namespace myredis {

// The server's key/value store. Facade over a Map<string, optional<string>>:
// callers (handlers, snapshotting) work with Get/Set/Del/SerialiseToJson
// rather than the generic Map interface, so the concrete map implementation
// stays an implementation detail of Store.
class Store {
 public:
  explicit Store(std::unique_ptr<Time> time);

  Store(const Store&) = delete;
  Store& operator=(const Store&) = delete;

  [[nodiscard]] std::optional<std::string> Get(const std::string& key) const;

  void Set(std::string key, std::optional<std::string> value);

  void Del(const std::string& key);

  // Follows the standard redis except NA means not applicable
  enum ExpireOption : std::uint8_t { NX, XX, GT, LT, NA };
  static std::optional<ExpireOption> ToExpireOption(const std::string& option) {
    if (option == "NX") return ExpireOption::NX;
    if (option == "XX") return ExpireOption::XX;
    if (option == "GT") return ExpireOption::GT;
    if (option == "LT") return ExpireOption::LT;
    return std::nullopt;
  }

  // timestamp is an absolute unix time in milliseconds (essentially
  // pexpireat) — callers compute it from whatever relative/absolute unit
  // their command takes.
  bool ExpireAt(const std::string& key, int64_t timestamp_ms);
  bool ExpireAt(const std::string& key, int64_t timestamp_ms,
                ExpireOption option);

  std::int64_t Ttl(const std::string& key);

  // Removes key's TTL. Returns false if the key doesn't exist (or has
  // already expired) or has no TTL to remove.
  bool Persist(const std::string& key);

  // Current store-observed time in milliseconds, for callers (e.g. the
  // EXPIRE/PEXPIRE handlers) that need to turn a relative TTL into the
  // absolute timestamp ExpireAt expects.
  [[nodiscard]] std::int64_t NowMs() const;

  // Serialises the store to a JSON object mapping each key to its value. A
  // key whose value is absent (std::nullopt) is serialised as JSON null.
  [[nodiscard]] std::string SerialiseToJson() const;

  // Populates the store from JSON produced by SerialiseToJson. Throws
  // std::invalid_argument if json_data is malformed.
  void DeserialiseFromJson(const std::string& json_data);

 private:
  static constexpr int64_t NO_EXPIRY = -1;

  struct Entry {
    int64_t expiry_ = NO_EXPIRY;
    std::optional<std::string> value_;
    explicit Entry(std::optional<std::string> value)
        : value_(std::move(value)) {}
    Entry(std::optional<std::string> value, int64_t expiry)
        : expiry_(expiry), value_(std::move(value)) {}
  };

  // Parses one {"value":...,"expiry":...} entry object at json_data[pos],
  // advancing pos past its closing '}'. Throws std::invalid_argument if
  // json_data is malformed.
  static Entry ParseEntryJson(const std::string& json_data, size_t& pos);

  std::unique_ptr<Map<std::string, Entry>> data_;
  std::unique_ptr<Time> time_;
};

}  // namespace myredis

#endif  // MYREDIS_STORE_STORE_H_
