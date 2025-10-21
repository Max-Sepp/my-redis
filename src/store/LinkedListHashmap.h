#ifndef MY_REDIS_LINKEDLISTHASHMAP_H
#define MY_REDIS_LINKEDLISTHASHMAP_H

#include <functional>

#include "store/Map.h"

#define DEFAULT_CAPACITY 16

template <typename K, typename V>
class LinkedListHashmap final : public Map<K, V> {
 public:
  LinkedListHashmap(const LinkedListHashmap &other)
      : Map<K, V>(other),
        hash_(other.hash_),
        size_(other.size),
        entries_(other.entries),
        load_factor_(other.load_factor_) {}

  LinkedListHashmap(LinkedListHashmap &&other) noexcept
      : Map<K, V>(std::move(other)),
        hash_(std::move(other.hash_)),
        size_(other.size),
        entries_(std::move(other.entries)),
        load_factor_(other.load_factor_) {}
  LinkedListHashmap &operator=(const LinkedListHashmap &other) {
    if (this == &other) return *this;
    Map<K, V>::operator=(other);
    hash_ = other.hash_;
    size_ = other.size;
    entries_ = other.entries;
    load_factor_ = other.load_factor_;
    return *this;
  }

  LinkedListHashmap &operator=(LinkedListHashmap &&other) noexcept {
    if (this == &other) return *this;
    Map<K, V>::operator=(std::move(other));
    hash_ = std::move(other.hash_);
    size_ = other.size;
    entries_ = std::move(other.entries);
    load_factor_ = other.load_factor_;
    return *this;
  }

  LinkedListHashmap(const double loadFactor,
                    std::function<size_t(const K &)> hash) {
    this->hash_ = hash;
    this->load_factor_ = loadFactor;
    this->entries_.resize(DEFAULT_CAPACITY);
  }

  LinkedListHashmap(const std::vector<std::pair<K, V>> &initialData,
                    const double loadFactor,
                    std::function<size_t(const K &)> hash) {
    this->hash_ = hash;
    this->load_factor_ = loadFactor;
    size_t cap = std::max(
        static_cast<size_t>(2),
        std::max(initialData.size_() * 2,
                 static_cast<size_t>(initialData.size_() / loadFactor)));
    this->entries.resize(cap);
    for (const auto &[key, value] : initialData) {
      this->insert(key, value);
    }
  }

  std::unique_ptr<V> LookUp(const K &key) override {
    if (entries_.empty()) return nullptr;
    Entry *currEntry = entries_[hash_(key) % entries_.size()].get();
    while (currEntry != nullptr && currEntry->key != key) {
      currEntry = currEntry->next.get();
    }
    if (currEntry != nullptr) {
      return std::make_unique<V>(currEntry->value);
    }
    return nullptr;
  }

  void Insert(const K &key, const V &value) override {
    if (entries_.empty() || size_ > entries_.size() * load_factor_) {
      Resize();
    }
    InsertWithoutResize(key, value);
  }

  void Remove(const K &key) override {
    if (entries_.empty()) return;
    const size_t bucketIndex = hash_(key) % entries_.size();
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

  void InsertWithoutResize(const K &key, const V &value) {
    const size_t bucket_index = hash_(key) % entries_.size();
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

#endif  // MY_REDIS_LINKEDLISTHASHMAP_H
