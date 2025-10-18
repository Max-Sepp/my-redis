#ifndef MY_REDIS_HASHMAP_H
#define MY_REDIS_HASHMAP_H

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

  std::function<size_t(const K &)> hash;
  double loadFactor;
  std::vector<Entry> entries;
  size_t size = 0;

 public:
  LinearProbingHashmap(const LinearProbingHashmap &other)
      : Map<K, V>(other),
        hash(other.hash),
        loadFactor(other.loadFactor),
        entries(other.entries),
        size(other.size) {}

  LinearProbingHashmap(LinearProbingHashmap &&other) noexcept
      : Map<K, V>(std::move(other)),
        hash(std::move(other.hash)),
        loadFactor(other.loadFactor),
        entries(std::move(other.entries)),
        size(other.size) {}

  LinearProbingHashmap &operator=(const LinearProbingHashmap &other) {
    if (this == &other) return *this;
    Map<K, V>::operator=(other);
    hash = other.hash;
    loadFactor = other.loadFactor;
    entries = other.entries;
    size = other.size;
    return *this;
  }

  LinearProbingHashmap &operator=(LinearProbingHashmap &&other) noexcept {
    if (this == &other) return *this;
    Map<K, V>::operator=(std::move(other));
    hash = std::move(other.hash);
    loadFactor = other.loadFactor;
    entries = std::move(other.entries);
    size = other.size;
    return *this;
  }

  LinearProbingHashmap(const double loadFactor,
                       std::function<size_t(const K &)> hash,
                       const size_t initialCapacity = DEFAULT_CAPACITY) {
    this->hash = hash;
    this->loadFactor = loadFactor;
    this->entries = std::vector<Entry>(
        initialCapacity,
        Entry{.state = empty, .key = std::nullopt, .value = std::nullopt});
  }

  LinearProbingHashmap(const std::vector<std::pair<K, V>> &initialData,
                       const double loadFactor,
                       std::function<size_t(const K &)> hash) {
    this->hash = hash;
    this->loadFactor = loadFactor;
    this->entries = std::vector<Entry>(
        std::max(
            static_cast<size_t>(2),
            std::max(initialData.size() * 2,
                     static_cast<size_t>(initialData.size() / loadFactor))),
        Entry{.state = empty});

    for (const auto &[key, value] : initialData) {
      this->insert(key, value);
    }
  }

  std::unique_ptr<V> lookUp(const K &key) override {
    const int valueBucketIndex = internalFind(key);

    if (valueBucketIndex != -1) {
      assert(entries[valueBucketIndex].value.has_value());
      return std::make_unique<V>(entries[valueBucketIndex].value.value());
    }

    return nullptr;
  }

  void insert(const K &key, const V &value) override {
    insertWithoutSize(key, value);
    size++;
  }

  void remove(const K &key) override {
    const int bucketIndex = internalFind(key);
    if (bucketIndex != -1) {
      entries[bucketIndex].state = deleted;
    }
  }

 private:
  void insertWithoutSize(const K &key, const V &value) {
    if (size > loadFactor * entries.size()) {
      resize();
    }

    size_t bucketIndex = hash(key) % entries.size();
    while (entries[bucketIndex].state == element &&
           entries[bucketIndex].key.has_value() &&
           entries[bucketIndex].key.value() != key) {
      bucketIndex = (bucketIndex + 1) % entries.size();
    }

    entries[bucketIndex].state = element;
    entries[bucketIndex].key = key;
    entries[bucketIndex].value = value;
  }

  void resize() {
    std::vector<Entry> oldEntries = entries;

    entries = std::vector<Entry>(
        std::max(static_cast<size_t>(2), oldEntries.size() * 2),
        Entry{.state = empty, .key = std::nullopt, .value = std::nullopt});

    for (const Entry &entry : oldEntries) {
      if (entry.state != element) continue;

      assert(entry.key.has_value() && entry.value.has_value());
      insertWithoutSize(entry.key.value(), entry.value.value());
    }
  }

  // Returns -1 if key cannot be found
  int internalFind(const K &key) {
    size_t bucketIndex = hash(key) % entries.size();
    const size_t initialBucketIndex = bucketIndex;

    while (entries[bucketIndex].state == deleted ||
           (entries[bucketIndex].state == element &&
            entries[bucketIndex].key.has_value() &&
            entries[bucketIndex].key.value() != key)) {
      bucketIndex = (bucketIndex + 1) % entries.size();
      if (bucketIndex == initialBucketIndex) {
        // Gonna through every element in the hashmap and could not find the key
        return -1;
      }
    }

    if (entries[bucketIndex].state == element) {
      return static_cast<int>(bucketIndex);
    }
    return -1;
  }
};

#endif  // MY_REDIS_HASHMAP_H
