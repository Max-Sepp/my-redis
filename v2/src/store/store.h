#ifndef MYREDIS_STORE_STORE_H_
#define MYREDIS_STORE_STORE_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <variant>

#include "data_structures/map/map.h"
#include "time/time.h"

namespace myredis {

// The server's key/value store. Facade over a Map<string, optional<string>>:
// callers (handlers, snapshotting) work with Get/Set/Del/SerialiseToJson
// rather than the generic Map interface, so the concrete map implementation
// stays an implementation detail of Store.
class Store {
 public:
  // rng_seed seeds the sampling in ActiveExpireCycle; tests pass a fixed one.
  explicit Store(std::unique_ptr<Time> time,
                 std::uint64_t rng_seed = std::random_device{}());

  Store(const Store&) = delete;
  Store& operator=(const Store&) = delete;

  // Returns key's value, or std::nullopt if it is missing or has expired. An
  // expired key is deleted on the spot.
  [[nodiscard]] std::optional<std::string> Get(const std::string& key);

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

  // Outcome of one sampling round of ActiveExpireCycle.
  struct ExpireRound {
    std::size_t sampled;
    std::size_t removed;
  };
  // Called after each round; returns true to sample another round.
  using ExpireContinuePredicate = std::function<bool(const ExpireRound&)>;

  struct ExpireCycleResult {
    std::size_t removed;
    // The final round, which may be partial if the index ran out of keys.
    ExpireRound last_round;
    // True if the cycle stopped because no keys with a TTL remain.
    bool index_empty;
  };

  // Actively removes expired keys by random sampling, like Redis's
  // activeExpireCycle: samples kExpireSampleSize keys that have a TTL, deletes
  // the expired ones, and samples again while should_continue allows (and
  // keys with a TTL remain). Always runs at least one round. Expired keys it
  // misses are deleted lazily when accessed. Scheduling policy (how long to
  // run, when to come back) belongs to the caller; see ExpiryTask.
  ExpireCycleResult ActiveExpireCycle(
      const ExpireContinuePredicate& should_continue);

  // Serialises the store to a JSON object mapping each key to its value. A
  // key whose value is absent (std::nullopt) is serialised as JSON null.
  // Keys that have expired but not yet been deleted are left out.
  [[nodiscard]] std::string SerialiseToJson() const;

  // Populates the store from JSON produced by SerialiseToJson. Throws
  // std::invalid_argument if json_data is malformed.
  void DeserialiseFromJson(const std::string& json_data);

 private:
  static constexpr int64_t NO_EXPIRY = -1;

  static constexpr std::size_t kExpireSampleSize = 20;

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

  static bool IsExpired(const Entry& entry, int64_t now_ms) {
    return entry.expiry_ != NO_EXPIRY && entry.expiry_ < now_ms;
  }

  // Deletes key if it has expired as of now_ms. Returns true if it was
  // deleted.
  bool RemoveIfExpired(const std::string& key, int64_t now_ms);

  // Looks key up, deleting it first if it has expired, so callers only ever
  // see live entries.
  std::optional<std::reference_wrapper<Entry>> LookUpLive(
      const std::string& key);

  std::unique_ptr<Map<std::string, Entry>> data_;
  // Index of the keys in data_ that have a TTL, so ActiveExpireCycle samples
  // only keys that can expire. The expiry itself stays in Entry; every
  // operation that adds or clears a TTL must keep this in sync.
  std::unique_ptr<Map<std::string, std::monostate>> expires_;
  std::unique_ptr<Time> time_;
  std::mt19937_64 rng_;
};

}  // namespace myredis

#endif  // MYREDIS_STORE_STORE_H_
