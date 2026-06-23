#ifndef MYREDIS_STORE_LINKED_LIST_HASHMAP_H_
#define MYREDIS_STORE_LINKED_LIST_HASHMAP_H_

#include <algorithm>
#include <cassert>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "store/map.h"

namespace myredis {

template <typename K, typename V>
class LinkedListHashmap final : public Map<K, V> {
  struct Entry {
    K key;
    V value;
    std::unique_ptr<Entry> next;
    Entry(K key, V value) : key(std::move(key)), value(std::move(value)) {}
  };

  std::function<size_t(const K&)> hash_;
  size_t size_ = 0;
  std::vector<std::unique_ptr<Entry>> entries_;
  double load_factor_;

  void InsertWithoutResize(K key, V value) {
    const size_t bucket_index = hash_(key) % entries_.size();
    if (entries_[bucket_index] == nullptr) {
      entries_[bucket_index] =
          std::make_unique<Entry>(std::move(key), std::move(value));
      size_++;
      return;
    }
    Entry* curr_entry = entries_[bucket_index].get();
    while (curr_entry->next != nullptr && curr_entry->key != key) {
      curr_entry = curr_entry->next.get();
    }
    if (curr_entry->key == key) {
      curr_entry->value = std::move(value);
    } else {
      assert(curr_entry->next == nullptr);
      curr_entry->next =
          std::make_unique<Entry>(std::move(key), std::move(value));
      size_++;
    }
  }

  void Resize() {
    std::vector<std::pair<K, V>> flat_entries;
    for (auto& bucket : entries_) {
      Entry* curr = bucket.get();
      while (curr) {
        flat_entries.emplace_back(std::move(curr->key), std::move(curr->value));
        curr = curr->next.get();
      }
    }
    entries_.clear();
    entries_.resize(std::max(static_cast<size_t>(2), size_ * 2));
    size_ = 0;
    for (auto& [key, value] : flat_entries) {
      InsertWithoutResize(std::move(key), std::move(value));
    }
  }

 public:
  // The map owns its entries through unique_ptr, so it is move-only.
  LinkedListHashmap(const LinkedListHashmap& other) = delete;
  LinkedListHashmap& operator=(const LinkedListHashmap& other) = delete;

  LinkedListHashmap(LinkedListHashmap&& other) noexcept
      : Map<K, V>(std::move(other)),
        hash_(std::move(other.hash_)),
        size_(other.size_),
        entries_(std::move(other.entries_)),
        load_factor_(other.load_factor_) {}

  LinkedListHashmap& operator=(LinkedListHashmap&& other) noexcept {
    if (this == &other) return *this;
    Map<K, V>::operator=(std::move(other));
    hash_ = std::move(other.hash_);
    size_ = other.size_;
    entries_ = std::move(other.entries_);
    load_factor_ = other.load_factor_;
    return *this;
  }

  LinkedListHashmap(const double load_factor,
                    std::function<size_t(const K&)> hash) {
    this->hash_ = std::move(hash);
    this->load_factor_ = load_factor;
    this->entries_.resize(kDefaultCapacity);
  }

  std::optional<std::reference_wrapper<const V>> LookUp(const K& key) override {
    if (entries_.empty()) return std::nullopt;
    Entry* curr_entry = entries_[hash_(key) % entries_.size()].get();
    while (curr_entry != nullptr && curr_entry->key != key) {
      curr_entry = curr_entry->next.get();
    }
    if (curr_entry != nullptr) {
      return std::optional<std::reference_wrapper<const V>>(
          std::cref(curr_entry->value));
    }
    return std::nullopt;
  }

  void Insert(K key, V value) override {
    if (entries_.empty() || size_ > entries_.size() * load_factor_) {
      Resize();
    }
    InsertWithoutResize(std::move(key), std::move(value));
  }

  void Remove(const K& key) override {
    if (entries_.empty()) return;
    const size_t bucket_index = hash_(key) % entries_.size();
    if (entries_[bucket_index] == nullptr) return;
    if (entries_[bucket_index]->key == key) {
      entries_[bucket_index] = std::move(entries_[bucket_index]->next);
      size_--;
      return;
    }
    Entry* curr_entry = entries_[bucket_index].get();
    while (curr_entry->next != nullptr && curr_entry->next->key != key) {
      curr_entry = curr_entry->next.get();
    }
    if (curr_entry->next != nullptr) {
      curr_entry->next = std::move(curr_entry->next->next);
      size_--;
    }
  }

  void ForEach(std::function<void(const K&, const V&)> action) override {
    for (const auto& bucket : entries_) {
      Entry* curr = bucket.get();
      while (curr) {
        action(curr->key, curr->value);
        curr = curr->next.get();
      }
    }
  }
};

}  // namespace myredis

#endif  // MYREDIS_STORE_LINKED_LIST_HASHMAP_H_
