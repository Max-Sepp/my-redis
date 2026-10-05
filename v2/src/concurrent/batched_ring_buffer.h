#ifndef MYREDIS_CONCURRENT_BATCHED_RING_BUFFER_H_
#define MYREDIS_CONCURRENT_BATCHED_RING_BUFFER_H_

#include <sys/mman.h>
#include <unistd.h>

#include <atomic>
#include <cstddef>
#include <cstdio>
#include <new>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace myredis {

// A single-producer single-consumer ring buffer of up to M elements, popped in
// contiguous batches. The storage is mapped twice back to back, so a batch
// that wraps past the end is still contiguous through the second mapping.
template <typename T, std::size_t M>
class BatchRingBuffer {
  static_assert(M > 0, "BatchRingBuffer must have a non-zero capacity");
  // Elements may be read through either mapping, which breaks types that point
  // into themselves (e.g. std::string's small-string buffer).
  static_assert(std::is_trivially_copyable_v<T>,
                "BatchRingBuffer elements must be trivially copyable");

 public:
  // Destroying the view frees its slots. Only one may be alive at a time.
  class ResultView {
   public:
    ResultView(const ResultView&) = delete;
    ResultView& operator=(const ResultView&) = delete;

    ResultView(ResultView&& other) noexcept
        : owner_(std::exchange(other.owner_, nullptr)),
          start_(other.start_),
          size_(other.size_) {}
    ResultView& operator=(ResultView&&) = delete;

    ~ResultView() {
      if (owner_ == nullptr) return;
      owner_->current_popping_ = false;
      const std::size_t head = owner_->head_.load(std::memory_order_relaxed);
      owner_->head_.store(head + size_, std::memory_order_release);
    }

    const T& operator[](std::size_t index) const {
      if (index >= size_) throw std::out_of_range("Access is out of range");
      return start_[index];
    }

    [[nodiscard]] const T* begin() const { return start_; }
    [[nodiscard]] const T* end() const { return start_ + size_; }
    [[nodiscard]] std::size_t size() const { return size_; }
    [[nodiscard]] bool empty() const { return size_ == 0; }

   private:
    friend class BatchRingBuffer<T, M>;

    ResultView(BatchRingBuffer<T, M>& owner, T* start, std::size_t size)
        : owner_(&owner), start_(start), size_(size) {}

    BatchRingBuffer<T, M>* owner_;  // nullptr once moved from
    T* start_;
    std::size_t size_;
  };

  BatchRingBuffer() : slot_count_(SlotCount()) {
    const std::size_t bytes = sizeof(T) * slot_count_;

    fd_ = memfd_create("batch_ring_buffer", MFD_CLOEXEC);
    if (fd_ == -1) {
      perror("memfd_create");
      throw std::runtime_error("Could not create the buffer's memory file");
    }
    if (ftruncate(fd_, static_cast<off_t>(bytes)) == -1) {
      perror("ftruncate");
      close(fd_);
      throw std::runtime_error("Could not size the buffer's memory file");
    }

    void* reservation =
        mmap(nullptr, bytes * 2, PROT_NONE,
             MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (reservation == MAP_FAILED) {
      perror("mmap reserve");
      close(fd_);
      throw std::runtime_error("Could not reserve the buffer's address range");
    }
    data_ = static_cast<T*>(reservation);

    for (std::size_t copy = 0; copy < 2; ++copy) {
      void* mapping =
          mmap(data_ + (copy * slot_count_), bytes, PROT_READ | PROT_WRITE,
               MAP_SHARED | MAP_FIXED, fd_, 0);
      if (mapping == MAP_FAILED) {
        perror("mmap fixed");
        munmap(data_, bytes * 2);
        close(fd_);
        throw std::runtime_error("Could not map the buffer");
      }
    }
  }

  BatchRingBuffer(const BatchRingBuffer&) = delete;
  BatchRingBuffer& operator=(const BatchRingBuffer&) = delete;
  BatchRingBuffer(BatchRingBuffer&&) = delete;
  BatchRingBuffer& operator=(BatchRingBuffer&&) = delete;

  ~BatchRingBuffer() {
    munmap(data_, sizeof(T) * slot_count_ * 2);
    close(fd_);
  }

  // Producer only. Returns false if the buffer is full.
  [[nodiscard]] bool Push(T element) {
    const std::size_t tail = tail_.load(std::memory_order_relaxed);
    if (tail - head_.load(std::memory_order_acquire) == M) return false;
    new (data_ + (tail % slot_count_)) T(element);
    tail_.store(tail + 1, std::memory_order_release);
    return true;
  }

  // Consumer only. Returns std::nullopt if fewer than `count` are buffered.
  [[nodiscard]] std::optional<ResultView> Pop(std::size_t count) {
    if (count > M) {
      throw std::out_of_range("Cannot pop more than the buffer's capacity");
    }
    if (current_popping_) {
      throw std::logic_error(
          "Cannot pop whilst another pop operation has not been completed");
    }

    const std::size_t head = head_.load(std::memory_order_relaxed);
    if (tail_.load(std::memory_order_acquire) - head < count) {
      return std::nullopt;
    }
    current_popping_ = true;
    return ResultView(*this, data_ + (head % slot_count_), count);
  }

  [[nodiscard]] std::size_t Size() const {
    // Load head_ first so the difference cannot underflow.
    const std::size_t head = head_.load(std::memory_order_acquire);
    return tail_.load(std::memory_order_acquire) - head;
  }

 private:
  // Each mapping must cover a whole number of pages and of elements. If
  // sizeof(T) * M is not such a multiple, it is rounded up and the spare
  // space becomes extra slots (e.g. 1000 ints round up to one 4096-byte page,
  // giving 1024 slots). Indices wrap modulo the slot count rather than M, but
  // Push still stops at M elements.
  static std::size_t SlotCount() {
    const auto page_size = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    const std::size_t granule = std::lcm(page_size, sizeof(T));
    const std::size_t bytes = (sizeof(T) * M + granule - 1) / granule * granule;
    return bytes / sizeof(T);
  }

  const std::size_t slot_count_;
  int fd_;
  T* data_;
  // Total elements ever popped and pushed; never wrapped.
  alignas(std::hardware_destructive_interference_size)
      std::atomic<std::size_t> head_{0};
  alignas(std::hardware_destructive_interference_size)
      std::atomic<std::size_t> tail_{0};
  bool current_popping_{false};
};

}  // namespace myredis

#endif  // MYREDIS_CONCURRENT_BATCHED_RING_BUFFER_H_
