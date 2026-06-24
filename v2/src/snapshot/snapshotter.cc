#include "snapshotter.h"

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include "store/serialise.h"

namespace myredis {
namespace {
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
                              std::to_string(timestamp) + ".snapshot.json"),
               store_json);
}

}  // namespace myredis
