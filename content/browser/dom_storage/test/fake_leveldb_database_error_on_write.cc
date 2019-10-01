// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/test/fake_leveldb_database_error_on_write.h"

#include "base/callback.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace content {
namespace test {

FakeLevelDBDatabaseErrorOnWrite::FakeLevelDBDatabaseErrorOnWrite(
    std::map<std::vector<uint8_t>, std::vector<uint8_t>>* mock_data)
    : FakeLevelDBDatabase(mock_data) {}

FakeLevelDBDatabaseErrorOnWrite::~FakeLevelDBDatabaseErrorOnWrite() = default;

void FakeLevelDBDatabaseErrorOnWrite::Write(
    std::vector<leveldb::mojom::BatchedOperationPtr> operations,
    WriteCallback callback) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                leveldb::mojom::DatabaseError::IO_ERROR));
}

}  // namespace test
}  // namespace content
