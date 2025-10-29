#ifndef MY_REDIS_HASHMAP_H
#define MY_REDIS_HASHMAP_H

#include <cassert>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "Map.h"

#define DEFAULT_CAPACITY 16

template <typename K, typename V>
class LinearProbingHashmap final : public Map<K, V> {
  enum State { empty, deleted, element };
  struct Entry {
    State state;
    std::optional<K> key;
    std::optional<V> value;
  };

  std::function<size_t(const K &)> hash_;
  double load_factor_;
  std::vector<Entry> entries_;
  size_t size_ = 0;

 public:
  LinearProbingHashmap(const LinearProbingHashmap &other)
      : Map<K, V>(other),
        hash_(other.hash_),
        load_factor_(other.load_factor_),
        entries_(other.entries_),
        size_(other.size_) {}

  LinearProbingHashmap(LinearProbingHashmap &&other) noexcept
      : Map<K, V>(std::move(other)),
        hash_(std::move(other.hash_)),
        load_factor_(other.load_factor_),
        entries_(std::move(other.entries_)),
        size_(other.size_) {}

  LinearProbingHashmap &operator=(const LinearProbingHashmap &other) {
    if (this == &other) return *this;
    Map<K, V>::operator=(other);
    hash_ = other.hash_;
    load_factor_ = other.load_factor_;
    entries_ = other.entries_;
    size_ = other.size_;
    return *this;
  }

  LinearProbingHashmap &operator=(LinearProbingHashmap &&other) noexcept {
    if (this == &other) return *this;
    Map<K, V>::operator=(std::move(other));
    hash_ = std::move(other.hash_);
    load_factor_ = other.load_factor_;
    entries_ = std::move(other.entries_);
    size_ = other.size_;
    return *this;
  }

  LinearProbingHashmap(const double load_factor,
                       std::function<size_t(const K &)> hash,
                       const size_t initial_capacity = DEFAULT_CAPACITY) {
    this->hash_ = hash;
    this->load_factor_ = load_factor;
    this->entries_ = std::vector<Entry>(
        initial_capacity,
        Entry{.state = empty, .key = std::nullopt, .value = std::nullopt});
  }

  LinearProbingHashmap(const std::vector<std::pair<K, V>> &initial_data,
                       const double load_factor,
                       std::function<size_t(const K &)> hash) {
    this->hash_ = hash;
    this->load_factor_ = load_factor;
    this->entries_ = std::vector<Entry>(
        std::max(
            static_cast<size_t>(2),
            std::max(initial_data.size_() * 2,
                     static_cast<size_t>(initial_data.size_() / load_factor))),
        Entry{.state = empty});

    for (const auto &[key, value] : initial_data) {
      this->insert(key, value);
    }
  }

  std::unique_ptr<V> LookUp(const K &key) override {
    const int value_bucket_index = InternalFind(key);

    if (value_bucket_index != -1) {
      assert(entries_[value_bucket_index].value.has_value());
      return std::make_unique<V>(entries_[value_bucket_index].value.value());
    }

    return nullptr;
  }

  void Insert(const K &key, const V &value) override {
    InsertWithoutSize(key, value);
    size_++;
  }

  void Remove(const K &key) override {
    const int bucket_index = InternalFind(key);
    if (bucket_index != -1) {
      entries_[bucket_index].state = deleted;
    }
  }

 private:
  void InsertWithoutSize(const K &key, const V &value) {
    if (size_ > load_factor_ * entries_.size()) {
      Resize();
    }

    size_t bucket_index = hash_(key) % entries_.size();
    while (entries_[bucket_index].state == element &&
           entries_[bucket_index].key.has_value() &&
           entries_[bucket_index].key.value() != key) {
      bucket_index = (bucket_index + 1) % entries_.size();
    }

    entries_[bucket_index].state = element;
    entries_[bucket_index].key = key;
    entries_[bucket_index].value = value;
  }

  void Resize() {
    std::vector<Entry> old_entries = entries_;

    entries_ = std::vector<Entry>(
        std::max(static_cast<size_t>(2), old_entries.size() * 2),
        Entry{.state = empty, .key = std::nullopt, .value = std::nullopt});

    for (const Entry &entry : old_entries) {
      if (entry.state != element) continue;

      assert(entry.key.has_value() && entry.value.has_value());
      InsertWithoutSize(entry.key.value(), entry.value.value());
    }
  }

  // Returns -1 if key cannot be found
  int InternalFind(const K &key) {
    size_t bucketIndex = hash_(key) % entries_.size();
    const size_t initial_bucket_index = bucketIndex;

    while (entries_[bucketIndex].state == deleted ||
           (entries_[bucketIndex].state == element &&
            entries_[bucketIndex].key.has_value() &&
            entries_[bucketIndex].key.value() != key)) {
      bucketIndex = (bucketIndex + 1) % entries_.size();
      if (bucketIndex == initial_bucket_index) {
        // Gonna through every element in the hashmap and could not find the key
        return -1;
      }
    }

    if (entries_[bucketIndex].state == element) {
      return static_cast<int>(bucketIndex);
    }
    return -1;
  }
};

#endif  // MY_REDIS_HASHMAP_H
