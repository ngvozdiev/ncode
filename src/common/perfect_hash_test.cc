#include "perfect_hash.h"

#include "gtest/gtest.h"

namespace ncode {
namespace {

struct ItemTag {};
using Store = PerfectHashStore<std::string, uint8_t, ItemTag>;
using Set = PerfectHashSet<uint8_t, ItemTag>;
using Map = PerfectHashMap<uint8_t, ItemTag, std::string>;

TEST(PerfectHash, Store) {
  Store store;

  std::string item = "SomeItem";
  auto index = store.AddItem(item);
  ASSERT_EQ(item, store.GetItemOrDie(index));

  Index<ItemTag, uint8_t> other_index(1);
  ASSERT_DEATH(store.GetItemOrDie(other_index), ".*");
  ASSERT_EQ(nullptr, store.GetItemOrNull(other_index));
}

TEST(PerfectHash, StoreTooManyItems) {
  Store store;
  for (size_t i = 0; i < 255; ++i) {
    store.AddItem(std::to_string(i));
  }

  ASSERT_DEATH(store.AddItem(std::to_string(256)), ".*");
}

TEST(PerfectHash, Set) {
  Store store;

  std::string item = "SomeItem";
  auto index = store.AddItem(item);

  std::string other_item = "OtherItem";
  auto other_index = store.AddItem(other_item);

  Set set;
  ASSERT_FALSE(set.Contains(index));
  ASSERT_FALSE(set.Contains(other_index));
  set.Insert(index);
  ASSERT_TRUE(set.Contains(index));
  ASSERT_FALSE(set.Contains(other_index));
}

TEST(PerfectHash, Map) {
  Store store;
  auto index = store.AddItem("SomeItem");

  Map map;
  map[index] = "HI";

  ASSERT_EQ("HI", map[index]);
  ASSERT_EQ("HI", map.GetValue(index));

  auto other_index = store.AddItem("OtherItem");
  ASSERT_EQ("", map.GetValue(other_index));
  ASSERT_EQ("", map[other_index]);
}

struct OtherItemTag {};
using StoreNotCopyable =
    PerfectHashStore<std::unique_ptr<std::string>, uint8_t, OtherItemTag>;

TEST(PerfectHashNotCopyable, Compile) {
  StoreNotCopyable store;

  auto item = make_unique<std::string>("SomeItem");
  auto index = store.MoveItem(std::move(item));
  ASSERT_EQ("SomeItem", *store.GetItemOrDie(index));
}

}  // namespace
}  // namespace ncode
