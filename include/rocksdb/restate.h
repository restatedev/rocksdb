//  Copyright (c) Restate Software, Inc.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

// Restate-specific C bindings for RocksDB.
// These are maintained separately to minimize merge conflicts with upstream.

#pragma once

#include "rocksdb/c.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Table Properties Collection type */
typedef struct rocksdb_table_properties_collection_t
    rocksdb_table_properties_collection_t;

/* Get properties of all tables in the default column family */
extern ROCKSDB_LIBRARY_API rocksdb_table_properties_collection_t*
rocksdb_get_properties_of_all_tables(rocksdb_t* db, char** errptr);

/* Get properties of all tables in a specific column family */
extern ROCKSDB_LIBRARY_API rocksdb_table_properties_collection_t*
rocksdb_get_properties_of_all_tables_cf(
    rocksdb_t* db, rocksdb_column_family_handle_t* column_family,
    char** errptr);

/* Get properties of tables within a key range in a specific column family.
 * The range is specified as [start_key, limit_key).
 * Pass NULL for both start_key and limit_key to get all tables.
 */
extern ROCKSDB_LIBRARY_API rocksdb_table_properties_collection_t*
rocksdb_get_properties_of_tables_in_range(
    rocksdb_t* db, rocksdb_column_family_handle_t* column_family,
    size_t num_ranges, const char* const* start_keys,
    const size_t* start_keys_lens, const char* const* limit_keys,
    const size_t* limit_keys_lens, char** errptr);

/* Get properties of tables organized by level.
 * Returns an array of table properties collections, one per level.
 * The caller is responsible for freeing each collection in the array
 * and the array itself using the provided destroy functions.
 */
extern ROCKSDB_LIBRARY_API void rocksdb_get_properties_of_tables_by_level(
    rocksdb_t* db, rocksdb_column_family_handle_t* column_family,
    rocksdb_table_properties_collection_t*** props_by_level, size_t* num_levels,
    char** errptr);

/* Destroy the array returned by rocksdb_get_properties_of_tables_by_level.
 * This destroys all collections in the array and the array itself.
 */
extern ROCKSDB_LIBRARY_API void
rocksdb_table_properties_collection_array_destroy(
    rocksdb_table_properties_collection_t** props_by_level, size_t num_levels);

/* Table Properties Collection accessors */

/* Get the number of tables in the collection */
extern ROCKSDB_LIBRARY_API size_t rocksdb_table_properties_collection_count(
    const rocksdb_table_properties_collection_t* collection);

/* Get the file name at the given index. Returns NULL if index is out of bounds.
 * The returned pointer is valid until the collection is destroyed.
 */
extern ROCKSDB_LIBRARY_API const char*
rocksdb_table_properties_collection_file_name(
    const rocksdb_table_properties_collection_t* collection, size_t index,
    size_t* name_len);

/* Get the table properties at the given index. Returns NULL if index is out of
 * bounds. The returned pointer is valid until the collection is destroyed.
 * Note: Do NOT call rocksdb_table_properties_destroy on the returned pointer;
 * it is owned by the collection.
 */
extern ROCKSDB_LIBRARY_API const rocksdb_table_properties_t*
rocksdb_table_properties_collection_properties(
    const rocksdb_table_properties_collection_t* collection, size_t index);

/* Destroy the table properties collection */
extern ROCKSDB_LIBRARY_API void rocksdb_table_properties_collection_destroy(
    rocksdb_table_properties_collection_t* collection);

/* Table Properties getters for numeric fields */
extern ROCKSDB_LIBRARY_API uint64_t
rocksdb_table_properties_get_orig_file_number(
    const rocksdb_table_properties_t* props);
extern ROCKSDB_LIBRARY_API uint64_t rocksdb_table_properties_get_data_size(
    const rocksdb_table_properties_t* props);
extern ROCKSDB_LIBRARY_API uint64_t rocksdb_table_properties_get_index_size(
    const rocksdb_table_properties_t* props);
extern ROCKSDB_LIBRARY_API uint64_t rocksdb_table_properties_get_filter_size(
    const rocksdb_table_properties_t* props);
extern ROCKSDB_LIBRARY_API uint64_t rocksdb_table_properties_get_raw_key_size(
    const rocksdb_table_properties_t* props);
extern ROCKSDB_LIBRARY_API uint64_t rocksdb_table_properties_get_raw_value_size(
    const rocksdb_table_properties_t* props);
extern ROCKSDB_LIBRARY_API uint64_t
rocksdb_table_properties_get_num_data_blocks(
    const rocksdb_table_properties_t* props);
extern ROCKSDB_LIBRARY_API uint64_t rocksdb_table_properties_get_num_entries(
    const rocksdb_table_properties_t* props);
extern ROCKSDB_LIBRARY_API uint64_t rocksdb_table_properties_get_num_deletions(
    const rocksdb_table_properties_t* props);
extern ROCKSDB_LIBRARY_API uint64_t
rocksdb_table_properties_get_num_merge_operands(
    const rocksdb_table_properties_t* props);
extern ROCKSDB_LIBRARY_API uint64_t
rocksdb_table_properties_get_num_range_deletions(
    const rocksdb_table_properties_t* props);
extern ROCKSDB_LIBRARY_API uint64_t rocksdb_table_properties_get_format_version(
    const rocksdb_table_properties_t* props);
extern ROCKSDB_LIBRARY_API uint64_t rocksdb_table_properties_get_fixed_key_len(
    const rocksdb_table_properties_t* props);
extern ROCKSDB_LIBRARY_API uint64_t
rocksdb_table_properties_get_column_family_id(
    const rocksdb_table_properties_t* props);
extern ROCKSDB_LIBRARY_API uint64_t rocksdb_table_properties_get_creation_time(
    const rocksdb_table_properties_t* props);
extern ROCKSDB_LIBRARY_API uint64_t
rocksdb_table_properties_get_oldest_key_time(
    const rocksdb_table_properties_t* props);
extern ROCKSDB_LIBRARY_API uint64_t
rocksdb_table_properties_get_file_creation_time(
    const rocksdb_table_properties_t* props);

/* Table Properties getters for string fields.
 * The returned pointer is valid until the properties object is destroyed.
 * If len is not NULL, the string length is stored there.
 */
extern ROCKSDB_LIBRARY_API const char* rocksdb_table_properties_get_db_id(
    const rocksdb_table_properties_t* props, size_t* len);
extern ROCKSDB_LIBRARY_API const char*
rocksdb_table_properties_get_db_session_id(
    const rocksdb_table_properties_t* props, size_t* len);
extern ROCKSDB_LIBRARY_API const char* rocksdb_table_properties_get_db_host_id(
    const rocksdb_table_properties_t* props, size_t* len);
extern ROCKSDB_LIBRARY_API const char*
rocksdb_table_properties_get_column_family_name(
    const rocksdb_table_properties_t* props, size_t* len);
extern ROCKSDB_LIBRARY_API const char*
rocksdb_table_properties_get_filter_policy_name(
    const rocksdb_table_properties_t* props, size_t* len);
extern ROCKSDB_LIBRARY_API const char*
rocksdb_table_properties_get_comparator_name(
    const rocksdb_table_properties_t* props, size_t* len);
extern ROCKSDB_LIBRARY_API const char*
rocksdb_table_properties_get_merge_operator_name(
    const rocksdb_table_properties_t* props, size_t* len);
extern ROCKSDB_LIBRARY_API const char*
rocksdb_table_properties_get_prefix_extractor_name(
    const rocksdb_table_properties_t* props, size_t* len);
extern ROCKSDB_LIBRARY_API const char*
rocksdb_table_properties_get_compression_name(
    const rocksdb_table_properties_t* props, size_t* len);

/* ============================================================================
 * SST File Reader - Read table properties directly from SST files
 * ============================================================================
 */

typedef struct rocksdb_sstfilereader_t rocksdb_sstfilereader_t;

/* Create an SST file reader with the given options.
 * The options object can be destroyed after creating the reader.
 */
extern ROCKSDB_LIBRARY_API rocksdb_sstfilereader_t* rocksdb_sstfilereader_create(
    const rocksdb_options_t* options);

/* Destroy the SST file reader */
extern ROCKSDB_LIBRARY_API void rocksdb_sstfilereader_destroy(
    rocksdb_sstfilereader_t* reader);

/* Open an SST file for reading.
 * The file_path should be the path to an existing SST file.
 */
extern ROCKSDB_LIBRARY_API void rocksdb_sstfilereader_open(
    rocksdb_sstfilereader_t* reader, const char* file_path, char** errptr);

/* Get table properties from the opened SST file.
 * Returns NULL if the file hasn't been opened or on error.
 * The returned properties must be destroyed with rocksdb_table_properties_destroy.
 * Note: Unlike properties from rocksdb_table_properties_collection_properties,
 * the caller owns this pointer and must destroy it.
 */
extern ROCKSDB_LIBRARY_API rocksdb_table_properties_t*
rocksdb_sstfilereader_get_table_properties(rocksdb_sstfilereader_t* reader);

/* Verify the checksum of the SST file */
extern ROCKSDB_LIBRARY_API void rocksdb_sstfilereader_verify_checksum(
    rocksdb_sstfilereader_t* reader, char** errptr);

/* Create an iterator over the SST file contents.
 * The iterator must be destroyed with rocksdb_iter_destroy.
 */
extern ROCKSDB_LIBRARY_API rocksdb_iterator_t* rocksdb_sstfilereader_new_iterator(
    rocksdb_sstfilereader_t* reader, const rocksdb_readoptions_t* options);

#ifdef __cplusplus
}
#endif
