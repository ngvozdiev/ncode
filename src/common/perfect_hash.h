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
  class Iterator {
   public:
    Iterator(PerfectHashMap<V, Tag, Value>* parent, Index<Tag, V> index)
        : parent_(parent), index_(index) {}
    Iterator operator++() {
      while (index_ != parent_->values_.size()) {
        index_ = Index<Tag, V>(index_ + 1);
        if (parent_->HasValue(index_)) {
          return *this;
        }
      }
      return *this;
    }
    bool operator!=(const Iterator& other) { return index_ != other.index_; }
    std::pair<Index<Tag, V>, Value*> operator*() const {
      return std::make_pair(index_, &parent_->values_[index_].second);
    }

   private:
    PerfectHashMap<V, Tag, Value>* parent_;
    Index<Tag, V> index_;
  };

  // Adds a new value.
  void Add(Index<Tag, V> index, Value value) {
    values_.resize(std::max(values_.size(), index + 1));
    values_[index] = {true, value};
  }

  // Returns a copy of the value associated with an index (or null_value) if no
  // value is associated with an index.
  const Value& GetValueOrDie(Index<Tag, V> index) const {
    CHECK(values_.size() > index);
    CHECK(values_[index].first);
    return values_[index].second;
  }

  bool HasValue(Index<Tag, V> index) const {
    if (values_.size() > index) {
      return values_[index].first;
    }

    return false;
  }

  Value& operator[](Index<Tag, V> index) {
    values_.resize(std::max(values_.size(), index + 1));
    std::pair<bool, Value>& bool_and_value = values_[index];
    bool_and_value.first = true;
    return bool_and_value.second;
  }

  const Value& operator[](Index<Tag, V> index) const {
    return GetValueOrDie(index);
  }

  Iterator begin() { return {this, Index<Tag, V>(0)}; }
  Iterator end() { return {this, Index<Tag, V>(values_.size())}; }

 private:
  std::vector<std::pair<bool, Value>> values_;
};
}

#endif
