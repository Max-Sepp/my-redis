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

 private:
  const std::filesystem::path output_dir_;
  const std::string snapshot_file_prefix_;
};

}  // namespace myredis

#endif  // MY_REDIS_SNAPSHOT_SNAPSHOTTER_H_
