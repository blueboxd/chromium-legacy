// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/leveldb/leveldb_env.h"

#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/optional.h"
#include "content/browser/indexed_db/indexed_db_leveldb_operations.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace indexed_db {
namespace {

TEST(LevelDBEnvTest, TestInMemory) {
  DefaultLevelDBFactory default_factory;
  scoped_refptr<LevelDBState> state;
  std::tie(state, std::ignore, std::ignore) = default_factory.OpenLevelDBState(
      base::FilePath(), GetDefaultLevelDBComparator(),
      /* create_if_missing=*/true);
  EXPECT_TRUE(state);
  EXPECT_TRUE(state->in_memory_env());
}

}  // namespace
}  // namespace indexed_db
}  // namespace content
