/*************************************************************************************************
 * On-memory database manager implementations based on hash table
 *
 * Copyright 2020 Google LLC
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License.  You may obtain a copy of the License at
 *     https://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied.  See the License for the specific language governing permissions
 * and limitations under the License.
 *************************************************************************************************/

#ifndef _TKRZW_DBM_TINY_H
#define _TKRZW_DBM_TINY_H

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <cinttypes>

#include "tkrzw_dbm.h"
#include "tkrzw_file.h"
#include "tkrzw_lib_common.h"

namespace tkrzw {

class TinyDBMImpl;
class TinyDBMIteratorImpl;

/**
 * On-memory database manager implementation based on hash table.
 * @details All operations are thread-safe; Multiple threads can access the same database
 * concurrently.
 */
class TinyDBM final : public DBM {
 public:
  /** The default value of the number of buckets. */
  static constexpr int64_t DEFAULT_NUM_BUCKETS = 1048583;

  /**
   * Iterator for each record.
   * @details When the database is updated, some iterators may or may not be invalided.
   * Operations with invalidated iterators fails gracefully with NOT_FOUND_ERROR.  One iterator
   * cannot be shared by multiple threads.
   */
  class Iterator final : public DBM::Iterator {
    friend class tkrzw::TinyDBM;
   public:
    /**
     * Destructor.
     */
    virtual ~Iterator();

    /**
     * Copy and assignment are disabled.
     */
    explicit Iterator(const Iterator& rhs) = delete;
    Iterator& operator =(const Iterator& rhs) = delete;

    /**
     * Initializes the iterator to indicate the first record.
     * @return The result status.
     * @details Even if there's no record, the operation doesn't fail.
     */
    Status First() override;

    /**
     * Initializes the iterator to indicate the last record.
     * @return The result status.
     * @details This method is not supported.
     */
    Status Last() override {
      return Status(Status::NOT_IMPLEMENTED_ERROR);
    }

    /**
     * Initializes the iterator to indicate a specific record.
     * @param key The key of the record to look for.
     * @return The result status.
     * @details This database is unordered so it doesn't support "lower bound" jump.  If there's
     * no record matching to the given key, the operation fails.
     */
    Status Jump(std::string_view key) override;

    /**
     * Initializes the iterator to indicate the last record whose key is lower than a given key.
     * @param key The key to compare with.
     * @param inclusive If true, the considtion is inclusive: equal to or lower than the key.
     * @return The result status.
     * @details This method is not supported.
     */
    Status JumpLower(std::string_view key, bool inclusive = false) override {
      return Status(Status::NOT_IMPLEMENTED_ERROR);
    }

    /**
     * Initializes the iterator to indicate the first record whose key is upper than a given key.
     * @param key The key to compare with.
     * @param inclusive If true, the considtion is inclusive: equal to or upper than the key.
     * @return The result status.
     * @details This method is not supported.
     */
    Status JumpUpper(std::string_view key, bool inclusive = false) override {
      return Status(Status::NOT_IMPLEMENTED_ERROR);
    }

    /**
     * Moves the iterator to the next record.
     * @return The result status.
     * @details If the current record is missing, the operation fails.  Even if there's no next
     * record, the operation doesn't fail.
     */
    Status Next() override;

    /**
     * Moves the iterator to the previous record.
     * @return The result status.
     * @details This method is not suppoerted.
     */
    Status Previous() override {
      return Status(Status::NOT_IMPLEMENTED_ERROR);
    }

    /**
     * Processes the current record with a processor.
     * @param proc The pointer to the processor object.
     * @param writable True if the processor can edit the record.
     * @return The result status.
     * @details If the current record exists, the ProcessFull of the processor is called.
     * Otherwise, this method fails and no method of the processor is called.  If the current
     * record is removed, the iterator is moved to the next record.
     */
    Status Process(RecordProcessor* proc, bool writable) override;

   private:
    /**
     * Constructor.
     * @param dbm_impl The database implementation object.
     */
    explicit Iterator(TinyDBMImpl* dbm_impl);

    /** Pointer to the actual implementation. */
    TinyDBMIteratorImpl* impl_;
  };

  /**
   * Default constructor.
   * @param num_buckets The number of buckets of the hash table.  -1 means that the default
   * value 1048583 is set.
   */
  explicit TinyDBM(int64_t num_buckets = -1);

  /**
   * Constructor with a file object.
   * @param file The file object to handle the data.  The ownership is taken.
   * @param num_buckets The number of buckets of the hash table.  -1 means that the default
   * value 1048583 is set.
   */
  explicit TinyDBM(std::unique_ptr<File> file, int64_t num_buckets = -1);

  /**
   * Destructor.
   */
  virtual ~TinyDBM();

  /**
   * Copy and assignment are disabled.
   */
  explicit TinyDBM(const TinyDBM& rhs) = delete;
  TinyDBM& operator =(const TinyDBM& rhs) = delete;

  /**
   * Opens a database file.
   * @param path A path of the file.
   * @param writable If true, the file is writable.  If false, it is read-only.
   * @param options Bit-sum options of File::OpenOption enums for opening the file.
   * @return The result status.
   * @details As this database is an on-memory database, you can set records and retrieve them
   * without opening a file.  If you open a file, records are loaded from the file.
   */
  Status Open(const std::string& path, bool writable,
              int32_t options = File::OPEN_DEFAULT) override;

  /**
   * Closes the database file.
   * @return The result status.
   * @details If a file is opened as writable, records are saved in the file.
   */
  Status Close() override;

  /**
   * Processes a record with a processor.
   * @param key The key of the record.
   * @param proc The pointer to the processor object.
   * @param writable True if the processor can edit the record.
   * @return The result status.
   * @details If the specified record exists, the ProcessFull of the processor is called.
   * Otherwise, the ProcessEmpty of the processor is called.
   */
  Status Process(std::string_view key, RecordProcessor* proc, bool writable) override;

  /**
   * Appends data at the end of a record of a key.
   * @param key The key of the record.
   * @param value The value to append.
   * @param delim The delimiter to put after the existing record.
   * @return The result status.
   * @details If there's no existing record, the value is set without the delimiter.
   */
  Status Append(
      std::string_view key, std::string_view value, std::string_view delim = "") override;

  /**
   * Processes multiple records with processors.
   * @param key_proc_pairs Pairs of the keys and their processor objects.
   * @param writable True if the processors can edit the records.
   * @return The result status.
   * @details Precondition: The database is opened.  The writable parameter should be
   * consistent to the open mode.
   * @details If the specified record exists, the ProcessFull of the processor is called.
   * Otherwise, the ProcessEmpty of the processor is called.
   */
  Status ProcessMulti(
      const std::vector<std::pair<std::string_view, DBM::RecordProcessor*>>& key_proc_pairs,
      bool writable) override;

  /**
   * Processes each and every record in the database with a processor.
   * @param proc The pointer to the processor object.
   * @param writable True if the processor can edit the record.
   * @return The result status.
   * @details The ProcessFull of the processor is called repeatedly for each record.  The
   * ProcessEmpty of the processor is called once before the iteration and once after the
   * iteration.
   */
  Status ProcessEach(RecordProcessor* proc, bool writable) override;

  /**
   * Gets the number of records.
   * @param count The pointer to an integer object to contain the result count.
   * @return The result status.
   */
  Status Count(int64_t* count) override;

  /**
   * Gets the current file size of the database.
   * @param size The pointer to an integer object to contain the result size.
   * @return The result status.
   */
  Status GetFileSize(int64_t* size) override;

  /**
   * Gets the path of the database file.
   * @param path The pointer to a string object to contain the result path.
   * @return The result status.
   */
  Status GetFilePath(std::string* path) override;

  /**
   * Removes all records.
   * @return The result status.
   */
  Status Clear() override;

  /**
   * Rebuilds the entire database.
   * @return The result status.
   * @details The number of buckets is calculated implicitly.
   */
  Status Rebuild() override {
    return RebuildAdvanced(-1);
  }

  /**
   * Rebuilds the entire database, in an advanced way.
   * @param num_buckets The number of buckets of the rebuild hash table.  -1 means that it is
   * calculated implicitly.
   * @return The result status.
   */
  Status RebuildAdvanced(int64_t num_buckets = -1);

  /**
   * Checks whether the database should be rebuilt.
   * @param tobe The pointer to a boolean object to contain the result decision.
   * @return The result status.
   */
  Status ShouldBeRebuilt(bool* tobe) override;

  /**
   * Synchronizes the content of the database to the file system.
   * @param hard True to do physical synchronization with the hardware or false to do only
   * logical synchronization with the file system.
   * @param proc The pointer to the file processor object, whose Process method is called while
   * the content of the file is synchronized.  If it is nullptr, it is ignored.
   * @return The result status.
   * @details If a file is opened as writable, records are saved in the file.
   */
  Status Synchronize(bool hard, FileProcessor* proc = nullptr) override;

  /**
   * Inspects the database.
   * @return A vector of pairs of a property name and its value.
   */
  std::vector<std::pair<std::string, std::string>> Inspect() override;

  /**
   * Checks whether the database is open.
   * @return True if the database is open, or false if not.
   */
  bool IsOpen() const override;

  /**
   * Checks whether the database is writable.
   * @return True if the database is writable, or false if not.
   */
  bool IsWritable() const override;

  /**
   * Checks whether the database condition is healthy.
   * @return Always true.  On-memory databases never cause system errors.
   */
  bool IsHealthy() const override {
    return true;
  }

  /**
   * Checks whether ordered operations are supported.
   * @return Always false.  Ordered operations are not supported.
   */
  bool IsOrdered() const override {
    return false;
  }

  /**
   * Makes an iterator for each record.
   * @return The iterator for each record.
   */
  std::unique_ptr<DBM::Iterator> MakeIterator() override;

  /**
   * Makes a new DBM object of the same concrete class.
   * @return The new DBM object.
   */
  std::unique_ptr<DBM> MakeDBM() const override;

 private:
  /** Pointer to the actual implementation. */
  TinyDBMImpl* impl_;
};

}  // namespace tkrzw

#endif  // _TKRZW_DBM_TINY_H

// END OF FILE
