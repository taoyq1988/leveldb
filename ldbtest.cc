// Copyright (c) 2024 LevelDB Test Utility

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <sstream>

#include "leveldb/db.h"
#include "leveldb/options.h"
#include "leveldb/write_batch.h"
#include "leveldb/iterator.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"

namespace leveldb {
namespace test {

class LevelDBTester {
 private:
  std::unique_ptr<leveldb::DB> db_;
  std::string db_path_;

 public:
  explicit LevelDBTester(const std::string& db_path) : db_path_(db_path) {}

  ~LevelDBTester() {
    if (db_) {
      db_.reset();
    }
  }

  bool Initialize(bool create_if_missing = true) {
    leveldb::Options options;
    options.create_if_missing = create_if_missing;
    options.write_buffer_size = 4 * 1024 * 1024;  // 4MB
    options.max_open_files = 1000;

    leveldb::DB* db_raw;
    leveldb::Status status = leveldb::DB::Open(options, db_path_, &db_raw);
    if (!status.ok()) {
      std::cerr << "Failed to open database: " << status.ToString() << std::endl;
      return false;
    }

    db_.reset(db_raw);
    std::cout << "Database opened successfully: " << db_path_ << std::endl;
    return true;
  }

  bool Put(const std::string& key, const std::string& value) {
    leveldb::Status status = db_->Put(leveldb::WriteOptions(), key, value);
    if (!status.ok()) {
      std::cerr << "Put failed: " << status.ToString() << std::endl;
      return false;
    }
    return true;
  }

  bool Get(const std::string& key, std::string* value) {
    leveldb::Status status = db_->Get(leveldb::ReadOptions(), key, value);
    if (status.IsNotFound()) {
      return false;
    } else if (!status.ok()) {
      std::cerr << "Get failed: " << status.ToString() << std::endl;
      return false;
    }
    return true;
  }

  bool Delete(const std::string& key) {
    leveldb::Status status = db_->Delete(leveldb::WriteOptions(), key);
    if (!status.ok()) {
      std::cerr << "Delete failed: " << status.ToString() << std::endl;
      return false;
    }
    return true;
  }

  bool BatchWrite(const std::vector<std::pair<std::string, std::string>>& kvs) {
    leveldb::WriteBatch batch;
    for (const auto& kv : kvs) {
      batch.Put(kv.first, kv.second);
    }

    leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);
    if (!status.ok()) {
      std::cerr << "Batch write failed: " << status.ToString() << std::endl;
      return false;
    }
    return true;
  }

  void Scan(const std::string& start_key = "", const std::string& end_key = "",
            int max_count = 100) {
    leveldb::ReadOptions read_options;
    std::unique_ptr<leveldb::Iterator> it(db_->NewIterator(read_options));

    int count = 0;
    if (start_key.empty()) {
      it->SeekToFirst();
    } else {
      it->Seek(start_key);
    }

    std::cout << "Scanning database:" << std::endl;
    while (it->Valid() && count < max_count) {
      leveldb::Slice key = it->key();
      leveldb::Slice value = it->value();

      if (!end_key.empty() && key.ToString() > end_key) {
        break;
      }

      std::cout << "  " << key.ToString() << " -> " << value.ToString() << std::endl;
      it->Next();
      count++;
    }

    if (!it->status().ok()) {
      std::cerr << "Iterator error: " << it->status().ToString() << std::endl;
    }

    std::cout << "Total " << count << " records scanned." << std::endl;
  }

  void GetStats() {
    std::string stats;
    if (db_->GetProperty("leveldb.stats", &stats)) {
      std::cout << "Database statistics:\n" << stats << std::endl;
    }

    std::string memory_usage;
    if (db_->GetProperty("leveldb.approximate-memory-usage", &memory_usage)) {
      std::cout << "Approximate memory usage: " << memory_usage << " bytes" << std::endl;
    }
  }

  void PerformanceTest(int num_operations = 10000) {
    std::cout << "Starting performance test with " << num_operations << " operations..." << std::endl;

    // Write test
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_operations; i++) {
      std::string key = "perf_key_" + std::to_string(i);
      std::string value = "perf_value_" + std::to_string(i) + "_" + std::string(100, 'x');
      if (!Put(key, value)) {
        std::cerr << "Performance test write failed at " << i << std::endl;
        return;
      }
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto write_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Read test
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_operations; i++) {
      std::string key = "perf_key_" + std::to_string(i);
      std::string value;
      if (!Get(key, &value)) {
        std::cerr << "Performance test read failed at " << i << std::endl;
        return;
      }
    }
    end = std::chrono::high_resolution_clock::now();
    auto read_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "Performance test results:" << std::endl;
    std::cout << "  Write: " << num_operations << " ops in " << write_duration.count() 
              << "ms (" << (num_operations * 1000.0 / write_duration.count()) << " ops/sec)" << std::endl;
    std::cout << "  Read:  " << num_operations << " ops in " << read_duration.count() 
              << "ms (" << (num_operations * 1000.0 / read_duration.count()) << " ops/sec)" << std::endl;
  }
};

}  // namespace test
}  // namespace leveldb

static void PrintUsage() {
  std::cout << "LevelDB Test Utility\n"
            << "Usage: ldbtest <db_path> <command> [args...]\n"
            << "Commands:\n"
            << "  put <key> <value>     - Put a key-value pair\n"
            << "  get <key>             - Get value by key\n"
            << "  delete <key>          - Delete a key\n"
            << "  scan [start] [end] [count] - Scan database\n"
            << "  batch <file>          - Batch load from file (key value per line)\n"
            << "  stats                 - Show database statistics\n"
            << "  perf [count]          - Run performance test\n"
            << "  help                  - Show this help\n"
            << "Examples:\n"
            << "  ldbtest ./testdb put hello world\n"
            << "  ldbtest ./testdb get hello\n"
            << "  ldbtest ./testdb scan\n"
            << "  ldbtest ./testdb scan user: user:9999 10\n"
            << "  ldbtest ./testdb perf 1000\n";
}

int main(int argc, char** argv) {
  if (argc < 3) {
    PrintUsage();
    return 1;
  }

  std::string db_path = argv[1];
  std::string command = argv[2];

  leveldb::test::LevelDBTester tester(db_path);
  if (!tester.Initialize()) {
    return 1;
  }

  if (command == "put") {
    if (argc != 5) {
      std::cerr << "Usage: ldbtest <db_path> put <key> <value>" << std::endl;
      return 1;
    }
    std::string key = argv[3];
    std::string value = argv[4];
    if (tester.Put(key, value)) {
      std::cout << "Put successful: " << key << " -> " << value << std::endl;
    } else {
      return 1;
    }
  } else if (command == "get") {
    if (argc != 4) {
      std::cerr << "Usage: ldbtest <db_path> get <key>" << std::endl;
      return 1;
    }
    std::string key = argv[3];
    std::string value;
    if (tester.Get(key, &value)) {
      std::cout << key << " -> " << value << std::endl;
    } else {
      std::cout << "Key not found: " << key << std::endl;
    }
  } else if (command == "delete") {
    if (argc != 4) {
      std::cerr << "Usage: ldbtest <db_path> delete <key>" << std::endl;
      return 1;
    }
    std::string key = argv[3];
    if (tester.Delete(key)) {
      std::cout << "Delete successful: " << key << std::endl;
    } else {
      return 1;
    }
  } else if (command == "scan") {
    std::string start_key = (argc > 3) ? argv[3] : "";
    std::string end_key = (argc > 4) ? argv[4] : "";
    int max_count = (argc > 5) ? std::atoi(argv[5]) : 100;
    tester.Scan(start_key, end_key, max_count);
  } else if (command == "stats") {
    tester.GetStats();
  } else if (command == "perf") {
    int count = (argc > 3) ? std::atoi(argv[3]) : 10000;
    tester.PerformanceTest(count);
  } else if (command == "help") {
    PrintUsage();
  } else {
    std::cerr << "Unknown command: " << command << std::endl;
    PrintUsage();
    return 1;
  }

  return 0;
}