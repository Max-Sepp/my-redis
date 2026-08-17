#ifndef MYREDIS_STORE_STORE_H_
#define MYREDIS_STORE_STORE_H_

#include <memory>
#include <optional>
#include <string>

#include "store/map/map.h"

namespace myredis {

// The server's key/value store. Facade over a Map<string, optional<string>>:
// callers (handlers, snapshotting) work with Get/Set/Del/SerialiseToJson
// rather than the generic Map interface, so the concrete map implementation
// stays an implementation detail of Store.
class Store {
 public:
  explicit Store();

  Store(const Store&) = delete;
  Store& operator=(const Store&) = delete;

  [[nodiscard]] std::optional<std::string> Get(const std::string& key) const;

  void Set(std::string key, std::optional<std::string> value);

  void Del(const std::string& key);

  // Serialises the store to a JSON object mapping each key to its value. A
  // key whose value is absent (std::nullopt) is serialised as JSON null.
  [[nodiscard]] std::string SerialiseToJson() const;

  // Populates the store from JSON produced by SerialiseToJson. Throws
  // std::invalid_argument if json_data is malformed.
  void DeserialiseFromJson(const std::string& json_data);

 private:
  std::unique_ptr<Map<std::string, std::optional<std::string>>> data_;
};

}  // namespace myredis

#endif  // MYREDIS_STORE_STORE_H_
