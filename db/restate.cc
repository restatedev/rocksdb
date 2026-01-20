//  Copyright (c) Restate Software, Inc.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

// Restate-specific C bindings implementation for RocksDB.
// These are maintained separately to minimize merge conflicts with upstream.

#include "rocksdb/restate.h"

#include <cstdlib>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "rocksdb/db.h"
#include "rocksdb/status.h"
#include "rocksdb/table_properties.h"
#include "rocksdb/types.h"

using ROCKSDB_NAMESPACE::ColumnFamilyHandle;
using ROCKSDB_NAMESPACE::DB;
using ROCKSDB_NAMESPACE::Range;
using ROCKSDB_NAMESPACE::Slice;
using ROCKSDB_NAMESPACE::Status;
using ROCKSDB_NAMESPACE::TableProperties;
using ROCKSDB_NAMESPACE::TablePropertiesCollection;

// Re-use the struct definitions from c.cc via forward declarations.
// These must match the definitions in db/c.cc exactly.
extern "C" {
struct rocksdb_t {
  DB* rep;
};
struct rocksdb_column_family_handle_t {
  ColumnFamilyHandle* rep;
  bool immortal;
};
struct rocksdb_table_properties_t {
  const TableProperties* rep;
};
}

// Internal structure to hold a TablePropertiesCollection and allow indexed
// access. The map is converted to a vector for O(1) index-based access.
struct rocksdb_table_properties_collection_t {
  // The original collection - we keep shared_ptrs alive
  TablePropertiesCollection collection;
  // Linearized for indexed access
  std::vector<std::pair<std::string, std::shared_ptr<const TableProperties>>>
      entries;

  void Linearize() {
    entries.clear();
    entries.reserve(collection.size());
    for (auto& kv : collection) {
      entries.emplace_back(kv.first, kv.second);
    }
  }
};

static bool SaveError(char** errptr, const Status& s) {
  if (s.ok()) {
    return false;
  }
  if (*errptr == nullptr) {
    *errptr = strdup(s.ToString().c_str());
  } else {
    free(*errptr);
    *errptr = strdup(s.ToString().c_str());
  }
  return true;
}

extern "C" {

rocksdb_table_properties_collection_t* rocksdb_get_properties_of_all_tables(
    rocksdb_t* db, char** errptr) {
  return rocksdb_get_properties_of_all_tables_cf(db, nullptr, errptr);
}

rocksdb_table_properties_collection_t* rocksdb_get_properties_of_all_tables_cf(
    rocksdb_t* db, rocksdb_column_family_handle_t* column_family,
    char** errptr) {
  auto* result = new rocksdb_table_properties_collection_t;
  ColumnFamilyHandle* cf =
      column_family ? column_family->rep : db->rep->DefaultColumnFamily();
  Status s = db->rep->GetPropertiesOfAllTables(cf, &result->collection);
  if (SaveError(errptr, s)) {
    delete result;
    return nullptr;
  }
  result->Linearize();
  return result;
}

rocksdb_table_properties_collection_t*
rocksdb_get_properties_of_tables_in_range(
    rocksdb_t* db, rocksdb_column_family_handle_t* column_family,
    size_t num_ranges, const char* const* start_keys,
    const size_t* start_keys_lens, const char* const* limit_keys,
    const size_t* limit_keys_lens, char** errptr) {
  if (num_ranges == 0) {
    // No ranges specified, return empty collection
    auto* result = new rocksdb_table_properties_collection_t;
    return result;
  }

  std::vector<Range> ranges;
  ranges.reserve(num_ranges);
  for (size_t i = 0; i < num_ranges; i++) {
    Slice start(start_keys[i], start_keys_lens[i]);
    Slice limit(limit_keys[i], limit_keys_lens[i]);
    ranges.emplace_back(start, limit);
  }

  auto* result = new rocksdb_table_properties_collection_t;
  ColumnFamilyHandle* cf =
      column_family ? column_family->rep : db->rep->DefaultColumnFamily();
  Status s = db->rep->GetPropertiesOfTablesInRange(cf, ranges.data(),
                                                   ranges.size(),
                                                   &result->collection);
  if (SaveError(errptr, s)) {
    delete result;
    return nullptr;
  }
  result->Linearize();
  return result;
}

void rocksdb_get_properties_of_tables_by_level(
    rocksdb_t* db, rocksdb_column_family_handle_t* column_family,
    rocksdb_table_properties_collection_t*** props_by_level, size_t* num_levels,
    char** errptr) {
  *props_by_level = nullptr;
  *num_levels = 0;

  std::vector<std::unique_ptr<TablePropertiesCollection>> cpp_props_by_level;
  ColumnFamilyHandle* cf =
      column_family ? column_family->rep : db->rep->DefaultColumnFamily();
  Status s = db->rep->GetPropertiesOfTablesByLevel(cf, &cpp_props_by_level);
  if (SaveError(errptr, s)) {
    return;
  }

  if (cpp_props_by_level.empty()) {
    return;
  }

  *num_levels = cpp_props_by_level.size();
  *props_by_level = static_cast<rocksdb_table_properties_collection_t**>(
      malloc(sizeof(rocksdb_table_properties_collection_t*) * (*num_levels)));

  for (size_t i = 0; i < *num_levels; i++) {
    auto* coll = new rocksdb_table_properties_collection_t;
    if (cpp_props_by_level[i]) {
      coll->collection = std::move(*cpp_props_by_level[i]);
      coll->Linearize();
    }
    (*props_by_level)[i] = coll;
  }
}

void rocksdb_table_properties_collection_array_destroy(
    rocksdb_table_properties_collection_t** props_by_level, size_t num_levels) {
  if (props_by_level == nullptr) {
    return;
  }
  for (size_t i = 0; i < num_levels; i++) {
    delete props_by_level[i];
  }
  free(props_by_level);
}

size_t rocksdb_table_properties_collection_count(
    const rocksdb_table_properties_collection_t* collection) {
  return collection->entries.size();
}

const char* rocksdb_table_properties_collection_file_name(
    const rocksdb_table_properties_collection_t* collection, size_t index,
    size_t* name_len) {
  if (index >= collection->entries.size()) {
    if (name_len) {
      *name_len = 0;
    }
    return nullptr;
  }
  const std::string& name = collection->entries[index].first;
  if (name_len) {
    *name_len = name.size();
  }
  return name.c_str();
}

// Thread-local cache for properties wrappers.
// We need to return rocksdb_table_properties_t* pointers that remain valid
// until the collection is destroyed. This cache holds wrapper objects that
// point into the collection's data.
static thread_local std::unordered_map<
    const rocksdb_table_properties_collection_t*,
    std::vector<rocksdb_table_properties_t>>
    g_props_cache;

const rocksdb_table_properties_t*
rocksdb_table_properties_collection_properties(
    const rocksdb_table_properties_collection_t* collection, size_t index) {
  if (index >= collection->entries.size()) {
    return nullptr;
  }

  // Lazy initialization of the properties wrapper cache
  auto& cache = g_props_cache[collection];
  if (cache.empty()) {
    cache.resize(collection->entries.size());
    for (size_t i = 0; i < collection->entries.size(); i++) {
      cache[i].rep = collection->entries[i].second.get();
    }
  }

  return &cache[index];
}

void rocksdb_table_properties_collection_destroy(
    rocksdb_table_properties_collection_t* collection) {
  // Clean up the cache entry for this collection
  g_props_cache.erase(collection);

  delete collection;
}

/* Table Properties getters - numeric fields */

uint64_t rocksdb_table_properties_get_orig_file_number(
    const rocksdb_table_properties_t* props) {
  return props->rep->orig_file_number;
}

uint64_t rocksdb_table_properties_get_data_size(
    const rocksdb_table_properties_t* props) {
  return props->rep->data_size;
}

uint64_t rocksdb_table_properties_get_index_size(
    const rocksdb_table_properties_t* props) {
  return props->rep->index_size;
}

uint64_t rocksdb_table_properties_get_filter_size(
    const rocksdb_table_properties_t* props) {
  return props->rep->filter_size;
}

uint64_t rocksdb_table_properties_get_raw_key_size(
    const rocksdb_table_properties_t* props) {
  return props->rep->raw_key_size;
}

uint64_t rocksdb_table_properties_get_raw_value_size(
    const rocksdb_table_properties_t* props) {
  return props->rep->raw_value_size;
}

uint64_t rocksdb_table_properties_get_num_data_blocks(
    const rocksdb_table_properties_t* props) {
  return props->rep->num_data_blocks;
}

uint64_t rocksdb_table_properties_get_num_entries(
    const rocksdb_table_properties_t* props) {
  return props->rep->num_entries;
}

uint64_t rocksdb_table_properties_get_num_deletions(
    const rocksdb_table_properties_t* props) {
  return props->rep->num_deletions;
}

uint64_t rocksdb_table_properties_get_num_merge_operands(
    const rocksdb_table_properties_t* props) {
  return props->rep->num_merge_operands;
}

uint64_t rocksdb_table_properties_get_num_range_deletions(
    const rocksdb_table_properties_t* props) {
  return props->rep->num_range_deletions;
}

uint64_t rocksdb_table_properties_get_format_version(
    const rocksdb_table_properties_t* props) {
  return props->rep->format_version;
}

uint64_t rocksdb_table_properties_get_fixed_key_len(
    const rocksdb_table_properties_t* props) {
  return props->rep->fixed_key_len;
}

uint64_t rocksdb_table_properties_get_column_family_id(
    const rocksdb_table_properties_t* props) {
  return props->rep->column_family_id;
}

uint64_t rocksdb_table_properties_get_creation_time(
    const rocksdb_table_properties_t* props) {
  return props->rep->creation_time;
}

uint64_t rocksdb_table_properties_get_oldest_key_time(
    const rocksdb_table_properties_t* props) {
  return props->rep->oldest_key_time;
}

uint64_t rocksdb_table_properties_get_file_creation_time(
    const rocksdb_table_properties_t* props) {
  return props->rep->file_creation_time;
}

/* Table Properties getters - string fields */

const char* rocksdb_table_properties_get_db_id(
    const rocksdb_table_properties_t* props, size_t* len) {
  if (len) {
    *len = props->rep->db_id.size();
  }
  return props->rep->db_id.c_str();
}

const char* rocksdb_table_properties_get_db_session_id(
    const rocksdb_table_properties_t* props, size_t* len) {
  if (len) {
    *len = props->rep->db_session_id.size();
  }
  return props->rep->db_session_id.c_str();
}

const char* rocksdb_table_properties_get_db_host_id(
    const rocksdb_table_properties_t* props, size_t* len) {
  if (len) {
    *len = props->rep->db_host_id.size();
  }
  return props->rep->db_host_id.c_str();
}

const char* rocksdb_table_properties_get_column_family_name(
    const rocksdb_table_properties_t* props, size_t* len) {
  if (len) {
    *len = props->rep->column_family_name.size();
  }
  return props->rep->column_family_name.c_str();
}

const char* rocksdb_table_properties_get_filter_policy_name(
    const rocksdb_table_properties_t* props, size_t* len) {
  if (len) {
    *len = props->rep->filter_policy_name.size();
  }
  return props->rep->filter_policy_name.c_str();
}

const char* rocksdb_table_properties_get_comparator_name(
    const rocksdb_table_properties_t* props, size_t* len) {
  if (len) {
    *len = props->rep->comparator_name.size();
  }
  return props->rep->comparator_name.c_str();
}

const char* rocksdb_table_properties_get_merge_operator_name(
    const rocksdb_table_properties_t* props, size_t* len) {
  if (len) {
    *len = props->rep->merge_operator_name.size();
  }
  return props->rep->merge_operator_name.c_str();
}

const char* rocksdb_table_properties_get_prefix_extractor_name(
    const rocksdb_table_properties_t* props, size_t* len) {
  if (len) {
    *len = props->rep->prefix_extractor_name.size();
  }
  return props->rep->prefix_extractor_name.c_str();
}

const char* rocksdb_table_properties_get_compression_name(
    const rocksdb_table_properties_t* props, size_t* len) {
  if (len) {
    *len = props->rep->compression_name.size();
  }
  return props->rep->compression_name.c_str();
}

}  // extern "C"
