#ifndef NCODE_PERFECT_HASH_H
#define NCODE_PERFECT_HASH_H

#include <limits>
#include <type_traits>
#include <vector>

#include "common.h"

namespace ncode {

// Assigns a unique incremental ID to items. This ID can later be used in
// perfect hashing sets and maps. The set of items cannot shrink -- once an item
// is added it cannot be removed. The first argument is the type of the items,
// the second argument is the type of the index. It should be an unsigned
// integer type. If more items are added than can be indexed with V will
// CHECK-fail. The third argument is a tag that will uniquely identify the
// items. This way two sets with the same item type will not have the same type.
template <typename T, typename V, typename Tag>
class PerfectHashStore {
 public:
  // Adds a copy of a new item to the set of items.
  Index<Tag, V> AddItem(T item) {
    CHECK(items_.size() < std::numeric_limits<V>::max());
    Index<Tag, V> next_index(items_.size());
    items_.emplace_back(item);
    return next_index;
  }

  // Adds and moves a new item to the set of items.
  Index<Tag, V> MoveItem(T&& item) {
    CHECK(items_.size() < std::numeric_limits<V>::max());
    Index<Tag, V> next_index(items_.size());
    items_.emplace_back(std::move(item));
    return next_index;
  }

  // Returns the address of the item that corresponds to an index. The address
  // may not be valid after additional calls to AddItem/MoveItem.
  const T* GetItemOrNull(Index<Tag, V> index) const {
    if (index >= items_.size()) {
      return nullptr;
    }

    return &items_[index];
  }

  const T& GetItemOrDie(Index<Tag, V> index) const {
    CHECK(index < items_.size());
    return items_[index];
  }

  size_t size() const { return items_.size(); }

 private:
  std::vector<T> items_;
};

// A set that contains indices with O(1) operations.
template <typename V, typename Tag>
class PerfectHashSet {
 public:
  void Insert(Index<Tag, V> index) {
    set_.resize(std::max(set_.size(), index + 1), false);
    set_[index] = true;
  }

  void Remove(Index<Tag, V> index) {
    if (set_.size() > index) {
      set_[index] = false;
    }
  }

  bool Contains(Index<Tag, V> index) const {
    if (set_.size() > index) {
      return set_[index];
    }

    return false;
  }

 private:
  std::vector<bool> set_;
};

// A map from index to a value with O(1) operations.
template <typename V, typename Tag, typename Value>
class PerfectHashMap {
 public:
  PerfectHashMap(Value null_value = Value()) : null_value_(null_value) {}

  // Adds a new value.
  void Add(Index<Tag, V> index, Value value) {
    values_.resize(std::max(values_.size(), index + 1), null_value_);
    values_[index] = value;
  }

  // Returns a copy of the value associated with an index (or null_value) if no
  // value is associated with an index.
  const Value& GetValue(Index<Tag, V> index) const {
    if (values_.size() > index) {
      return values_[index];
    }

    return null_value_;
  }

  Value& operator[](Index<Tag, V> index) {
    values_.resize(std::max(values_.size(), index + 1), null_value_);
    return values_[index];
  }

  const Value& operator[](Index<Tag, V> index) const { return GetValue(index); }

 private:
  Value null_value_;

  std::vector<Value> values_;
};
}

#endif
