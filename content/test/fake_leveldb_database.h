// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_FAKE_LEVELDB_DATABASE_H_
#define CONTENT_TEST_FAKE_LEVELDB_DATABASE_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "components/services/leveldb/public/mojom/leveldb.mojom.h"

namespace content {

// Simple implementation of the leveldb::mojom::LevelDBDatabase interface that
// is backed by a std::map.

class FakeLevelDBDatabase : public leveldb::mojom::LevelDBDatabase {
 public:
  // |mock_data| must not be null and must outlive this MockLevelDBDatabase
  // instance. All callbacks are called synchronously.
  explicit FakeLevelDBDatabase(
      std::map<std::vector<uint8_t>, std::vector<uint8_t>>* mock_data);
  ~FakeLevelDBDatabase() override;

  void SetDestructionCallback(base::OnceClosure callback) {
    destruction_callback_ = std::move(callback);
  }

  // LevelDBDatabase:
  void Put(const std::vector<uint8_t>& key,
           const std::vector<uint8_t>& value,
           PutCallback callback) override;
  void Delete(const std::vector<uint8_t>& key,
              DeleteCallback callback) override;
  void DeletePrefixed(const std::vector<uint8_t>& key_prefix,
                      DeletePrefixedCallback callback) override;
  void RewriteDB(RewriteDBCallback callback) override;
  void Write(std::vector<leveldb::mojom::BatchedOperationPtr> operations,
             WriteCallback callback) override;
  void Get(const std::vector<uint8_t>& key, GetCallback callback) override;
  void GetPrefixed(const std::vector<uint8_t>& key_prefix,
                   GetPrefixedCallback callback) override;
  void GetMany(std::vector<leveldb::mojom::GetManyRequestPtr> keys_or_prefixes,
               GetManyCallback callback) override;
  void CopyPrefixed(const std::vector<uint8_t>& source_key_prefix,
                    const std::vector<uint8_t>& destination_key_prefix,
                    CopyPrefixedCallback callback) override;

 private:
  std::vector<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>
  CopyPrefixedHelper(const std::vector<uint8_t>& source_key_prefix,
                     const std::vector<uint8_t>& destination_key_prefix);

  std::map<std::vector<uint8_t>, std::vector<uint8_t>>& mock_data_;
  base::OnceClosure destruction_callback_;
};

}  // namespace content

#endif  // CONTENT_TEST_FAKE_LEVELDB_DATABASE_H_
