#include "snapshotter.h"

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "store/serialise.h"

namespace myredis {
namespace {

constexpr std::string_view kSnapshotSuffix = ".snapshot.json";

// Returns the path of the newest snapshot in `dir`: the file named
// "<prefix><timestamp>.snapshot.json" with the largest timestamp, or nullopt if
// there is none. The timestamp is compared numerically, not lexically, so the
// millisecond counts sort correctly across digit-count boundaries. Leftover
// ".tmp-*" files from an interrupted write don't carry the prefix and so are
// ignored.
std::optional<std::filesystem::path> LatestSnapshot(
    const std::filesystem::path& dir, const std::string& prefix) {
  std::optional<std::filesystem::path> best;
  long long best_timestamp = -1;

  std::error_code dir_error;
  for (const auto& entry : std::filesystem::directory_iterator(dir, dir_error)) {
    if (!entry.is_regular_file()) continue;
    const std::string name = entry.path().filename().string();
    if (!name.starts_with(prefix) || !name.ends_with(kSnapshotSuffix)) continue;

    const char* first = name.data() + prefix.size();
    const char* last = name.data() + name.size() - kSnapshotSuffix.size();
    long long timestamp = 0;
    const auto [ptr, errc] = std::from_chars(first, last, timestamp);
    if (errc != std::errc() || ptr != last) continue;  // not all digits

    if (timestamp > best_timestamp) {
      best_timestamp = timestamp;
      best = entry.path();
    }
  }
  return best;
}

void DurableWrite(const std::filesystem::path& output_path,
                  const std::string& output) {
  std::filesystem::path dir = output_path.parent_path();
  if (dir.empty()) {
    dir = ".";
  }

  // Use a std::string so mkstemp can mutate the template in place, rather than
  // const_cast-ing path::c_str() (which would be undefined behaviour).
  std::string tmp_file = (dir / ".tmp-XXXXXX").string();

  int temp_fd = mkstemp(tmp_file.data());
  if (temp_fd == -1) throw std::runtime_error("mkstemp failed");

  try {
    // Write everything to the temp file
    const char* output_ptr = output.data();
    size_t left = output.size();

    while (left > 0) {
      ssize_t bytes_written = write(temp_fd, output_ptr, left);
      if (bytes_written == -1) {
        if (errno == EINTR) continue;
        throw std::runtime_error("write failed");
      }
      output_ptr += bytes_written;
      left -= static_cast<size_t>(bytes_written);
    }

    // Flush temp file to non volatile storage
    if (fsync(temp_fd) == -1) throw std::runtime_error("fsync(file) failed");

    // Close the temp file
    if (close(temp_fd) == -1) {
      temp_fd = -1;
      throw std::runtime_error("close failed");
    }

    temp_fd = -1;

    // Atomically replace the target
    if (rename(tmp_file.c_str(), output_path.c_str()) == -1)
      throw std::runtime_error("rename failed");

    // fsync the directory so the rename (a directory metadata change) is itself
    // durable; without this a crash after rename could lose the file entry.
    int dir_fd = open(dir.c_str(), O_RDONLY | O_DIRECTORY);
    if (dir_fd == -1) throw std::runtime_error("open(dir) failed");
    if (fsync(dir_fd) == -1) {
      close(dir_fd);
      throw std::runtime_error("fsync(dir) failed");
    }
    if (close(dir_fd) == -1) throw std::runtime_error("close(dir) failed");
  } catch (...) {
    if (temp_fd != -1) close(temp_fd);
    unlink(tmp_file.c_str());  // don't leave a stray temp behind
    throw;
  }
}
}  // namespace

void Snapshotter::Snapshot(
    const std::unique_ptr<Map<std::string, std::optional<std::string>>>&
        store) {
  const std::string store_json = SerialiseMapToJson(store);

  auto timestamp = duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();

  DurableWrite(output_dir_ / (snapshot_file_prefix_ +
                              std::to_string(timestamp) +
                              std::string(kSnapshotSuffix)),
               store_json);
}

bool Snapshotter::Restore(
    std::unique_ptr<Map<std::string, std::optional<std::string>>>& store)
    const {
  const std::optional<std::filesystem::path> path =
      LatestSnapshot(output_dir_, snapshot_file_prefix_);
  if (!path) return true;

  std::ifstream stream(*path, std::ios::binary);
  if (!stream) {
    std::cerr << "Could not open snapshot " << *path << "\n";
    return false;
  }
  std::ostringstream buffer;
  buffer << stream.rdbuf();

  try {
    DeserialiseJsonToMap(store, buffer.str());
  } catch (const std::exception& e) {
    std::cerr << "Could not parse snapshot " << *path << ": " << e.what()
              << "\n";
    return false;
  }

  std::cout << "Restored store from snapshot " << *path << "\n";
  return true;
}

}  // namespace myredis
