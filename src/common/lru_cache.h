#ifndef NCODE_LRU_H
#define NCODE_LRU_H

#include <stddef.h>
#include <functional>
#include <list>
#include <memory>
#include <unordered_map>
#include <utility>

#include "common.h"
#include "map_util.h"

namespace ncode {

// An LRU cache that maps K to V.
template <typename K, typename V, class Hash = std::hash<K>,
          class Pred = std::equal_to<K>>
class LRUCache {
 public:
  virtual ~LRUCache() {}
  LRUCache(size_t max_cache_size) : max_cache_size_(max_cache_size) {}

  // Inserts a new item in the cache, or inserts a new one with the given
  // arguments to the constructor. This call will only result in a constructor
  // being called if there is no entry associated with 'key'.
  template <class... Args>
  V& Emplace(const K& key, Args&&... args) {
    ObjectAndListIterator& object_and_list_it = cache_map_[key];
    if (object_and_list_it.object) {
      // No insertion took place, key was already in cache. Will just update
      // keys_.
      typename LRUList::iterator list_it = object_and_list_it.iterator;
      keys_.splice(keys_.begin(), keys_, list_it);
      return *object_and_list_it.object;
    }

    if (cache_map_.size() > max_cache_size_) {
      EvictOldest();
    }

    // Insertion took place. Need to add the key to the front of the list and
    // actually construct the value.
    keys_.emplace_front(key);
    object_and_list_it.object = make_unique<V>(args...);
    object_and_list_it.iterator = keys_.begin();
    return *object_and_list_it.object;
  }

  V* FindOrNull(const K& key) {
    ObjectAndListIterator* object_and_list_it =
        ncode::FindOrNull(cache_map_, key);
    if (object_and_list_it == nullptr) {
      return nullptr;
    }

    CHECK(object_and_list_it->object);
    return object_and_list_it->object.get();
  }

  const V* FindOrNull(const K& key) const {
    const ObjectAndListIterator* object_and_list_it =
        ncode::FindOrNull(cache_map_, key);
    if (object_and_list_it == nullptr) {
      return nullptr;
    }

    CHECK(object_and_list_it->object);
    return object_and_list_it->object.get();
  }

  // Evicts the entire cache.
  void EvictAll() {
    while (cache_map_.size()) {
      EvictOldest();
    }
  }

  // Called when an item is evicted from the cache.
  virtual void ItemEvicted(const K& key, std::unique_ptr<V> value) {
    Unused(key);
    Unused(value);
  };

 private:
  using LRUList = std::list<K>;
  struct ObjectAndListIterator {
    ObjectAndListIterator() {}

    std::unique_ptr<V> object;
    typename LRUList::iterator iterator;
  };
  using CacheMap = std::unordered_map<K, ObjectAndListIterator, Hash, Pred>;

  void EvictOldest() {
    // Need to evict an entry.
    const K& to_evict = keys_.back();
    auto it = cache_map_.find(to_evict);
    CHECK(it != cache_map_.end());

    std::unique_ptr<V> to_evict_value = std::move(it->second.object);
    ItemEvicted(to_evict, std::move(to_evict_value));

    cache_map_.erase(it);
    keys_.pop_back();
  }

  const size_t max_cache_size_;

  LRUList keys_;
  CacheMap cache_map_;
};

}  // namespace ncode

#endif
