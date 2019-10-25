// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/leveldb/leveldb_env.h"

#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/optional.h"
#include "content/browser/indexed_db/leveldb/leveldb_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/include/leveldb/comparator.h"

namespace content {
namespace indexed_db {
namespace {
leveldb_env::Options GetLevelDBOptions() {
  static base::NoDestructor<leveldb_env::ChromiumEnv> test_env;
  static const leveldb::FilterPolicy* kFilterPolicy =
      leveldb::NewBloomFilterPolicy(10);
  leveldb_env::Options options;
  options.comparator = leveldb::BytewiseComparator();
  options.paranoid_checks = true;
  options.filter_policy = kFilterPolicy;
  options.compression = leveldb::kSnappyCompression;
  options.env = test_env.get();
  return options;
}

TEST(LevelDBEnvTest, TestInMemory) {
  DefaultLevelDBFactory default_factory(GetLevelDBOptions(),
                                        "test-in-memory-db");
  scoped_refptr<LevelDBState> state;
  std::tie(state, std::ignore, std::ignore) = default_factory.OpenLevelDBState(
      base::FilePath(), leveldb::BytewiseComparator(),
      /* create_if_missing=*/true);
  EXPECT_TRUE(state);
  EXPECT_TRUE(state->in_memory_env());
}

}  // namespace
}  // namespace indexed_db
}  // namespace content
