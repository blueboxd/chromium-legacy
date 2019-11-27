// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/games/core/data_files_parser.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "components/games/core/games_utils.h"
#include "components/games/core/proto/games_catalog.pb.h"
#include "components/games/core/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace games {

class DataFilesParserTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  DataFilesParser parser_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(DataFilesParserTest, TryParseCatalog_FileDoesNotExist) {
  GamesCatalog catalog;
  EXPECT_FALSE(parser_.TryParseCatalog(temp_dir_.GetPath(), &catalog));
}

TEST_F(DataFilesParserTest, TryParseCatalog_Success) {
  GamesCatalog expected_catalog = test::CreateGamesCatalogWithOneGame();
  std::string serialized_proto = expected_catalog.SerializeAsString();
  const auto catalog_path = GetGamesCatalogPath(temp_dir_.GetPath());

  ASSERT_NE(-1, base::WriteFile(catalog_path, serialized_proto.data(),
                                serialized_proto.size()));

  GamesCatalog test_catalog;
  EXPECT_TRUE(parser_.TryParseCatalog(temp_dir_.GetPath(), &test_catalog));
  EXPECT_TRUE(test::AreProtosEqual(expected_catalog, test_catalog));
}

}  // namespace games
