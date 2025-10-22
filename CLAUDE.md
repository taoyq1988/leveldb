# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

LevelDB is a fast key-value storage library written at Google that provides an ordered mapping from string keys to string values. This is a foundational storage engine used by many projects.

**Important**: This repository receives very limited maintenance. Only critical bug fixes (data loss, memory corruption) and changes needed by internally supported clients are reviewed.

## Build and Test Commands

### Building

```bash
# Create build directory and configure
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release .. && cmake --build .
```

For debug builds:
```bash
cmake -DCMAKE_BUILD_TYPE=Debug .. && cmake --build .
```

### Running Tests

```bash
# From build directory
cd build

# Run all tests
./leveldb_tests

# Run specific test (use CTest)
ctest -R <test_name>

# Run a specific standalone test
./c_test              # C API test
./env_posix_test      # POSIX environment test (on Unix)
./env_windows_test    # Windows environment test (on Windows)
```

### Running Benchmarks

```bash
# From build directory
cd build
./db_bench           # Main benchmark suite
```

### Code Formatting

This project follows the Google C++ Style Guide. Before committing:

```bash
clang-format -i --style=file <file>
```

## Architecture Overview

### Core Components

**Public API** (`include/leveldb/`):
- `db.h` - Main database interface with Put/Get/Delete/Write operations
- `options.h` - Configuration for database behavior and read/write operations
- `write_batch.h` - Atomic batch operations
- `iterator.h` - Forward/backward iteration over data
- `slice.h` - Lightweight reference to byte arrays (avoids copying)
- `status.h` - Error handling and status reporting
- `env.h` - OS abstraction layer for file operations
- `cache.h` - LRU cache for uncompressed blocks
- `comparator.h` - Custom key ordering
- `filter_policy.h` - Bloom filters to reduce disk reads

**Database Implementation** (`db/`):
- `db_impl.{cc,h}` - Core database implementation
- `memtable.{cc,h}` - In-memory write buffer (backed by skiplist)
- `skiplist.h` - Lock-free skiplist for memtable
- `log_writer.{cc,h}` / `log_reader.{cc,h}` - Write-ahead log (WAL)
- `version_set.{cc,h}` / `version_edit.{cc,h}` - Multi-version concurrency control, tracks SSTable files
- `table_cache.{cc,h}` - Caches open SSTable file handles
- `write_batch.cc` - Implementation of atomic batch writes

**Sorted Tables** (`table/`):
- `table.cc` / `table_builder.cc` - SSTable file format (*.ldb files)
- `block.{cc,h}` / `block_builder.{cc,h}` - Block-based storage within SSTables
- `filter_block.{cc,h}` - Bloom filter blocks
- `format.{cc,h}` - SSTable file format definitions
- `merger.{cc,h}` - Multi-way merge for reading from multiple SSTables
- `two_level_iterator.{cc,h}` - Iterator over index blocks and data blocks

**Utilities** (`util/`):
- `env_posix.cc` / `env_windows.cc` - Platform-specific file I/O
- `cache.cc` - LRU cache implementation
- `bloom.cc` - Bloom filter implementation
- `arena.{cc,h}` - Memory allocator for memtable
- `coding.{cc,h}` - Varint and fixed-width integer encoding
- `crc32c.{cc,h}` - Checksums for data integrity

**Portability** (`port/`):
- `port_stdcxx.h` - Standard C++ portability layer
- `thread_annotations.h` - Thread safety annotations for Clang

### Storage Architecture

LevelDB uses a Log-Structured Merge (LSM) tree:

1. **Writes** go to:
   - Write-Ahead Log (*.log) - for crash recovery
   - MemTable (in-memory skiplist) - for fast writes

2. **When MemTable fills** (~4MB):
   - Converted to immutable MemTable
   - Flushed to Level-0 SSTable (*.ldb file)
   - New MemTable created

3. **Compaction** (background):
   - Level-0: Files may have overlapping keys (4 files threshold)
   - Level-N (N≥1): Non-overlapping key ranges
   - Level-N size limits: 10^N MB (10MB for L1, 100MB for L2, etc.)
   - Compaction merges files from Level-N to Level-(N+1)
   - Drops deleted/overwritten values

4. **Reads** check:
   - MemTable
   - Immutable MemTable (if exists)
   - Level-0 files (may need to check all)
   - Level-1+ files (binary search by key range)

5. **Metadata**:
   - MANIFEST - Lists all SSTable files and their levels
   - CURRENT - Points to active MANIFEST file
   - LOG/LOG.old - Informational logs

### Concurrency Model

- Single process per database (file lock enforced)
- Thread-safe for concurrent operations on same `DB` object
- No external synchronization needed for reads/writes
- Iterators and WriteBatch need external locking if shared across threads

### Key Abstractions

**Slice**: Lightweight view into byte arrays - just pointer + length. Used everywhere to avoid copying data.

**Status**: Return type for operations that can fail. Check with `status.ok()` and get error message with `status.ToString()`.

**Snapshot**: Immutable view of database state at a point in time. Used for consistent reads.

**WriteBatch**: Atomic batch of Put/Delete operations. Applied as single transaction.

## Development Notes

### Testing Requirements

All changes must include tests. Test files are located in:
- `db/*_test.cc` - Database core tests
- `table/*_test.cc` - SSTable tests
- `util/*_test.cc` - Utility tests

### Build Configuration

CMake options in `CMakeLists.txt`:
- `LEVELDB_BUILD_TESTS` (default ON) - Build test suite
- `LEVELDB_BUILD_BENCHMARKS` (default ON) - Build benchmarks
- `LEVELDB_INSTALL` (default ON) - Install headers and library

Optional dependencies (auto-detected):
- snappy - Fast compression (recommended)
- zstd - Alternative compression
- crc32c - Hardware-accelerated CRC32C
- tcmalloc - Faster memory allocation

### Important Constraints

- C++17 required (set in CMakeLists.txt)
- No exceptions (disabled in build)
- No RTTI (disabled in build)
- Platform support: POSIX (Linux/macOS) and Windows only
- Stable API - avoid breaking changes
- Must handle arbitrary byte arrays (including '\0' in keys/values)

### Code Organization

The codebase separates concerns cleanly:
- Public headers never include internal implementation details
- Platform-specific code isolated in `util/env_*.cc` and `port/`
- `helpers/memenv/` provides in-memory Env implementation for testing
- Tests use GoogleTest framework (`third_party/googletest/`)
- Benchmarks use Google Benchmark (`third_party/benchmark/`)

## Documentation

- `doc/index.md` - Complete API usage guide
- `doc/impl.md` - Implementation details and LSM tree architecture
- `doc/table_format.md` - SSTable file format specification
- `doc/log_format.md` - Write-ahead log format specification
