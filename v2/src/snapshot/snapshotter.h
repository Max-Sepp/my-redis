#ifndef MY_REDIS_SNAPSHOT_SNAPSHOTTER_H_
#define MY_REDIS_SNAPSHOT_SNAPSHOTTER_H_

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "store/map.h"

namespace myredis {

class Snapshotter {
 public:
  Snapshotter(std::filesystem::path output_dir,
              std::string snapshot_file_prefix)
      : output_dir_(std::move(output_dir)),
        snapshot_file_prefix_(std::move(snapshot_file_prefix)) {}

  void Snapshot(
      const std::unique_ptr<Map<std::string, std::optional<std::string>>>&
          store);

  // Restores `store` from the newest snapshot this Snapshotter would have
  // written: the file in output_dir_ named "<prefix><timestamp>.snapshot.json"
  // with the largest timestamp. Returns false only if such a snapshot exists
  // but cannot be read or parsed; a missing snapshot is a normal first run and
  // returns true (leaving `store` untouched).
  bool Restore(
      std::unique_ptr<Map<std::string, std::optional<std::string>>>& store)
      const;

 private:
  const std::filesystem::path output_dir_;
  const std::string snapshot_file_prefix_;
};

}  // namespace myredis

#endif  // MY_REDIS_SNAPSHOT_SNAPSHOTTER_H_
