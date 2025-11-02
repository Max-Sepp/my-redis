#ifndef MY_REDIS_STRIPEDHASHMAP_H
#define MY_REDIS_STRIPEDHASHMAP_H

#include <cassert>
#include <functional>
#include <mutex>

#include "store/Map.h"

#define DEFAULT_CAPACITY 16

template <typename K, typename V>
class StripedHashmap final : public Map<K, V> {
 public:
  StripedHashmap(const std::vector<std::pair<K, V>> &initialData,
                 const double load_factor,
                 std::function<size_t(const K &)> hash)
      : StripedHashmap(initialData, load_factor, hash, DEFAULT_CAPACITY) {}

  StripedHashmap(const double load_factor,
                 std::function<size_t(const K &)> hash)
      : StripedHashmap(load_factor, hash, DEFAULT_CAPACITY) {}

  StripedHashmap(const double load_factor,
                 std::function<size_t(const K &)> hash, const size_t num_locks)
      : StripedHashmap({}, load_factor, hash, num_locks) {}

  StripedHashmap(const std::vector<std::pair<K, V>> &initialData,
                 const double load_factor,
                 std::function<size_t(const K &)> hash, const size_t num_locks)
      : hash_(std::move(hash)), load_factor_(load_factor), locks_(num_locks) {
    size_t cap = std::max(
        static_cast<size_t>(2),
        std::max(initialData.size() * 2,
                 static_cast<size_t>(initialData.size() / load_factor)));
    this->entries_.resize(cap);
    for (const auto &[key, value] : initialData) {
      this->Insert(key, value);
    }
  }

  std::unique_ptr<V> LookUp(const K &key) override {
    std::lock_guard lock(locks_[GetLockIndex(key)]);

    if (entries_.empty()) return nullptr;
    Entry *currEntry = entries_[GetBucketIndex(key)].get();
    while (currEntry != nullptr && currEntry->key != key) {
      currEntry = currEntry->next.get();
    }
    if (currEntry != nullptr) {
      return std::make_unique<V>(currEntry->value);
    }
    return nullptr;
  }

  void Insert(const K &key, const V &value) override {
    std::lock_guard lock(locks_[GetLockIndex(key)]);

    if (entries_.empty() || size_ > entries_.size() * load_factor_) Resize();

    InsertWithoutResize(key, value);
  }

  void Remove(const K &key) override {
    std::lock_guard lock(locks_[GetLockIndex(key)]);

    if (entries_.empty()) return;
    const size_t bucketIndex = GetBucketIndex(key);
    if (entries_[bucketIndex] == nullptr) return;
    if (entries_[bucketIndex]->key == key) {
      entries_[bucketIndex] = std::move(entries_[bucketIndex]->next);
      size_--;
      return;
    }
    Entry *currEntry = entries_[bucketIndex].get();
    while (currEntry->next != nullptr && currEntry->next->key != key) {
      currEntry = currEntry->next.get();
    }
    if (currEntry->next != nullptr) {
      currEntry->next = std::move(currEntry->next->next);
      size_--;
    }
  }

 private:
  struct Entry {
    K key;
    V value;
    std::unique_ptr<Entry> next;
    Entry(const K &key, const V &value) : key(key), value(value) {}
  };

  std::function<size_t(const K &)> hash_;
  size_t size_ = 0;
  std::vector<std::unique_ptr<Entry>> entries_;
  double load_factor_;
  std::vector<std::recursive_mutex> locks_;

  size_t GetBucketIndex(const K &key) { return hash_(key) % entries_.size(); }
  size_t GetLockIndex(const K &key) { return hash_(key) % locks_.size(); }

  void InsertWithoutResize(const K &key, const V &value) {
    const size_t bucket_index = GetBucketIndex(key);

    if (entries_[bucket_index] == nullptr) {
      entries_[bucket_index] = std::make_unique<Entry>(key, value);
      size_++;
      return;
    }
    Entry *curr_entry = entries_[bucket_index].get();
    while (curr_entry->next != nullptr && curr_entry->key != key) {
      curr_entry = curr_entry->next.get();
    }
    if (curr_entry->key == key) {
      curr_entry->value = value;
    } else {
      assert(curr_entry->next == nullptr);
      curr_entry->next = std::make_unique<Entry>(key, value);
      size_++;
    }
  }

  void Resize() {
    // Acquire all locks
    std::vector<std::unique_lock<std::recursive_mutex>> locks;
    locks.reserve(locks_.size());
    for (std::recursive_mutex &l : locks_) locks.emplace_back(l);

    std::vector<std::pair<K, V>> flat_entries;
    for (auto &bucket : entries_) {
      Entry *curr = bucket.get();
      while (curr) {
        flat_entries.push_back({curr->key, curr->value});
        curr = curr->next.get();
      }
    }
    entries_.clear();
    entries_.resize(std::max(static_cast<size_t>(2), size_ * 2));
    size_ = 0;
    for (const auto &[key, value] : flat_entries) {
      InsertWithoutResize(key, value);
    }
  }
};

#endif  // MY_REDIS_STRIPEDHASHMAP_H
