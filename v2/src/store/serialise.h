#ifndef MYREDIS_STORE_SERIALISE_H_
#define MYREDIS_STORE_SERIALISE_H_

#include <memory>
#include <optional>
#include <string>

#include "store/map.h"

namespace myredis {

// Serialises the store to a JSON object mapping each key to its value. A key
// whose value is absent (std::nullopt) is serialised as JSON null.
std::string SerialiseMapToJson(
    const std::unique_ptr<Map<std::string, std::optional<std::string>>>& store);

// Deserialise the JSON into store.
void DeserialiseJsonToMap(
    std::unique_ptr<Map<std::string, std::optional<std::string>>>& store,
    const std::string& json_data);

}  // namespace myredis

#endif  // MYREDIS_STORE_SERIALISE_H_
