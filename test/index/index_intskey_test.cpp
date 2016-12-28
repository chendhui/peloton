//===----------------------------------------------------------------------===//
//
//                         Peloton
//
// index_test.cpp
//
// Identification: test/index/index_test.cpp
//
// Copyright (c) 2015-16, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "common/harness.h"
#include "gtest/gtest.h"

#include "common/logger.h"
#include "common/platform.h"
#include "index/index_factory.h"
#include "storage/tuple.h"

namespace peloton {
namespace test {

//===--------------------------------------------------------------------===//
// Index IntsKey Tests
//===--------------------------------------------------------------------===//

class IndexIntsKeyTests : public PelotonTest {};

catalog::Schema *key_schema = nullptr;
catalog::Schema *tuple_schema = nullptr;

const int NUM_TUPLES = 10;

/*
 * BuildIndex()
 */
index::Index *BuildIndex(IndexType index_type, const bool unique_keys,
                         std::vector<type::Type::TypeId> col_types) {
  // Build tuple and key schema
  std::vector<catalog::Column> column_list;
  std::vector<oid_t> key_attrs;

  char column_char = 'A';
  int num_cols = (int)col_types.size();
  for (int i = 0; i < num_cols; i++) {
    std::ostringstream os;
    os << static_cast<char>((int)column_char + i);

    catalog::Column column(col_types[i], type::Type::GetTypeSize(col_types[i]),
                           os.str(), true);
    column_list.push_back(column);
    key_attrs.push_back(i);
  }  // FOR
  key_schema = new catalog::Schema(column_list);
  key_schema->SetIndexedColumns(key_attrs);
  tuple_schema = new catalog::Schema(column_list);

  // Build index metadata
  //
  // NOTE: Since here we use a relatively small key (size = 12)
  // so index_test is only testing with a certain kind of key
  // (most likely, GenericKey)
  //
  // For testing IntsKey and TupleKey we need more test cases
  index::IndexMetadata *index_metadata = new index::IndexMetadata(
      "MAGIC_TEST_INDEX", 125,  // Index oid
      INVALID_OID, INVALID_OID, index_type, INDEX_CONSTRAINT_TYPE_DEFAULT,
      tuple_schema, key_schema, key_attrs, unique_keys);

  // Build index
  // The type of index key has been chosen inside this function, but we are
  // unable to know the exact type of key it has chosen
  index::Index *index = index::IndexFactory::GetIndex(index_metadata);

  // Actually this will never be hit since if index creation fails an exception
  // would be raised (maybe out of memory would result in a nullptr? Anyway
  // leave it here)
  EXPECT_TRUE(index != NULL);

  return index;
}

void IndexIntsKeyTestHelper(IndexType index_type,
                            std::vector<type::Type::TypeId> col_types) {
  auto pool = TestingHarness::GetInstance().GetTestingPool();
  std::vector<ItemPointer *> location_ptrs;

  // CREATE
  std::unique_ptr<index::Index> index(BuildIndex(index_type, false, col_types));

  // POPULATE
  std::vector<std::shared_ptr<storage::Tuple>> keys;
  std::vector<std::shared_ptr<ItemPointer>> items;
  for (int i = 0; i < NUM_TUPLES; i++) {
    std::shared_ptr<storage::Tuple> key(new storage::Tuple(key_schema, true));
    std::shared_ptr<ItemPointer> item(new ItemPointer(i, i * i));

    for (int col_idx = 0; col_idx < (int)col_types.size(); col_idx++) {
      int val = (10 * i) + col_idx;
      switch (col_types[col_idx]) {
        case type::Type::TINYINT: {
          key->SetValue(col_idx, type::ValueFactory::GetTinyIntValue(val),
                        pool);
          break;
        }
        case type::Type::SMALLINT: {
          key->SetValue(col_idx, type::ValueFactory::GetSmallIntValue(val),
                        pool);
          break;
        }
        case type::Type::INTEGER: {
          key->SetValue(col_idx, type::ValueFactory::GetIntegerValue(val),
                        pool);
          break;
        }
        case type::Type::BIGINT: {
          key->SetValue(col_idx, type::ValueFactory::GetBigIntValue(val), pool);
          break;
        }
        default:
          throw peloton::Exception("Unexpected type!");
      }

      // INSERT
      index->InsertEntry(key.get(), item.get());

      keys.push_back(key);
      items.push_back(item);
    }  // FOR

    // SCAN
    for (int i = 0; i < NUM_TUPLES; i++) {
      location_ptrs.clear();
      index->ScanKey(keys[i].get(), location_ptrs);
      EXPECT_EQ(location_ptrs.size(), 1);
      EXPECT_EQ(location_ptrs[0]->block, items[i]->block);
    }  // FOR

    // DELETE
    for (int i = 0; i < NUM_TUPLES; i++) {
      index->DeleteEntry(keys[i].get(), items[i].get());
      location_ptrs.clear();
      index->ScanKey(keys[i].get(), location_ptrs);
      EXPECT_EQ(location_ptrs.size(), 0);
    }  // FOR

    delete tuple_schema;
  }  // FOR
}

TEST_F(IndexIntsKeyTests, BwTreeTest) {
  std::vector<type::Type::TypeId> types = {
      type::Type::BIGINT, type::Type::INTEGER, type::Type::SMALLINT,
      type::Type::TINYINT};

  // I know that there is a more elegant way to do this but I don't
  // have time for that right now...

  // ONE COLUMN
  for (type::Type::TypeId type0 : types) {
    std::vector<type::Type::TypeId> col_types = {type0};
    IndexIntsKeyTestHelper(INDEX_TYPE_BWTREE, col_types);
  }
  // TWO COLUMNS
  for (type::Type::TypeId type0 : types) {
    for (type::Type::TypeId type1 : types) {
      std::vector<type::Type::TypeId> col_types = {type0, type1};
      IndexIntsKeyTestHelper(INDEX_TYPE_BWTREE, col_types);
    }
  }
  // THREE COLUMNS
  for (type::Type::TypeId type0 : types) {
    for (type::Type::TypeId type1 : types) {
      for (type::Type::TypeId type2 : types) {
        std::vector<type::Type::TypeId> col_types = {type0, type1, type2};
        IndexIntsKeyTestHelper(INDEX_TYPE_BWTREE, col_types);
      }
    }
  }
  // FOUR COLUMNS
  for (type::Type::TypeId type0 : types) {
    for (type::Type::TypeId type1 : types) {
      for (type::Type::TypeId type2 : types) {
        for (type::Type::TypeId type3 : types) {
          std::vector<type::Type::TypeId> col_types = {type0, type1, type2,
                                                       type3};
          IndexIntsKeyTestHelper(INDEX_TYPE_BWTREE, col_types);
        }
      }
    }
  }
}

// FIXME: The B-Tree core dumps. If we're not going to support then we should
// probably drop it.
// TEST_F(IndexIntsKeyTests, BTreeTest) {
// IndexIntsKeyHelper(INDEX_TYPE_BTREE);
// }

}  // End test namespace
}  // End peloton namespace
