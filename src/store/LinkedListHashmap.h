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
        hash(other.hash),
        size(other.size),
        entries(other.entries),
        loadFactor(other.loadFactor) {}

  LinkedListHashmap(LinkedListHashmap &&other) noexcept
      : Map<K, V>(std::move(other)),
        hash(std::move(other.hash)),
        size(other.size),
        entries(std::move(other.entries)),
        loadFactor(other.loadFactor) {}
  LinkedListHashmap &operator=(const LinkedListHashmap &other) {
    if (this == &other) return *this;
    Map<K, V>::operator=(other);
    hash = other.hash;
    size = other.size;
    entries = other.entries;
    loadFactor = other.loadFactor;
    return *this;
  }

  LinkedListHashmap &operator=(LinkedListHashmap &&other) noexcept {
    if (this == &other) return *this;
    Map<K, V>::operator=(std::move(other));
    hash = std::move(other.hash);
    size = other.size;
    entries = std::move(other.entries);
    loadFactor = other.loadFactor;
    return *this;
  }

  LinkedListHashmap(const double loadFactor,
                    std::function<size_t(const K &)> hash) {
    this->hash = hash;
    this->loadFactor = loadFactor;
    this->entries.resize(DEFAULT_CAPACITY);
  }

  LinkedListHashmap(const std::vector<std::pair<K, V>> &initialData,
                    const double loadFactor,
                    std::function<size_t(const K &)> hash) {
    this->hash = hash;
    this->loadFactor = loadFactor;
    size_t cap = std::max(
        static_cast<size_t>(2),
        std::max(initialData.size() * 2,
                 static_cast<size_t>(initialData.size() / loadFactor)));
    this->entries.resize(cap);
    for (const auto &[key, value] : initialData) {
      this->insert(key, value);
    }
  }

  std::unique_ptr<V> lookUp(const K &key) override {
    if (entries.empty()) return nullptr;
    Entry *currEntry = entries[hash(key) % entries.size()].get();
    while (currEntry != nullptr && currEntry->key != key) {
      currEntry = currEntry->next.get();
    }
    if (currEntry != nullptr) {
      return std::make_unique<V>(currEntry->value);
    }
    return nullptr;
  }

  void insert(const K &key, const V &value) override {
    if (entries.empty() || size > entries.size() * loadFactor) {
      resize();
    }
    insertWithoutResize(key, value);
  }

  void remove(const K &key) override {
    if (entries.empty()) return;
    const size_t bucketIndex = hash(key) % entries.size();
    if (entries[bucketIndex] == nullptr) return;
    if (entries[bucketIndex]->key == key) {
      entries[bucketIndex] = std::move(entries[bucketIndex]->next);
      size--;
      return;
    }
    Entry *currEntry = entries[bucketIndex].get();
    while (currEntry->next != nullptr && currEntry->next->key != key) {
      currEntry = currEntry->next.get();
    }
    if (currEntry->next != nullptr) {
      currEntry->next = std::move(currEntry->next->next);
      size--;
    }
  }

 private:
  struct Entry {
    K key;
    V value;
    std::unique_ptr<Entry> next;
    Entry(const K &key, const V &value) : key(key), value(value) {}
  };

  std::function<size_t(const K &)> hash;
  size_t size = 0;
  std::vector<std::unique_ptr<Entry>> entries;
  double loadFactor;

  void insertWithoutResize(const K &key, const V &value) {
    const size_t bucketIndex = hash(key) % entries.size();
    if (entries[bucketIndex] == nullptr) {
      entries[bucketIndex] = std::make_unique<Entry>(key, value);
      size++;
      return;
    }
    Entry *currEntry = entries[bucketIndex].get();
    while (currEntry->next != nullptr && currEntry->key != key) {
      currEntry = currEntry->next.get();
    }
    if (currEntry->key == key) {
      currEntry->value = value;
    } else {
      assert(currEntry->next == nullptr);
      currEntry->next = std::make_unique<Entry>(key, value);
      size++;
    }
  }

  void resize() {
    std::vector<std::pair<K, V>> flatEntries;
    for (auto &bucket : entries) {
      Entry *curr = bucket.get();
      while (curr) {
        flatEntries.push_back({curr->key, curr->value});
        curr = curr->next.get();
      }
    }
    entries.clear();
    entries.resize(std::max(static_cast<size_t>(2), size * 2));
    size = 0;
    for (const auto &[key, value] : flatEntries) {
      insertWithoutResize(key, value);
    }
  }
};

#endif  // MY_REDIS_LINKEDLISTHASHMAP_H
