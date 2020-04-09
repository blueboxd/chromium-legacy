// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_logs/debug_daemon_log_source.h"

#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/rand_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace system_logs {

class DebugDaemonLogSourceTest : public ::testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::ScopedTempDir temp_dir_;
};

TEST_F(DebugDaemonLogSourceTest, ReadEndOfFileEmpty) {
  std::string read_data("should be erased");

  base::FilePath file_path = temp_dir_.GetPath().Append("test_empty.txt");

  WriteFile(file_path, "", 0);

  EXPECT_TRUE(ReadEndOfFile(file_path, &read_data, 10));
  EXPECT_EQ(0u, read_data.length());
}

TEST_F(DebugDaemonLogSourceTest, ReadEndOfFileSmall) {
  const char kTestData[] = "0123456789";  // Length of 10
  std::string read_data;

  base::FilePath file_path = temp_dir_.GetPath().Append("test.txt");

  WriteFile(file_path, kTestData, strlen(kTestData));

  read_data.clear();
  EXPECT_TRUE(ReadEndOfFile(file_path, &read_data, 15));
  EXPECT_EQ(kTestData, read_data);

  read_data.clear();
  EXPECT_TRUE(ReadEndOfFile(file_path, &read_data, 10));
  EXPECT_EQ(kTestData, read_data);

  read_data.clear();
  EXPECT_TRUE(ReadEndOfFile(file_path, &read_data, 2));
  EXPECT_EQ("89", read_data);

  read_data.clear();
  EXPECT_TRUE(ReadEndOfFile(file_path, &read_data, 3));
  EXPECT_EQ("789", read_data);

  read_data.clear();
  EXPECT_TRUE(ReadEndOfFile(file_path, &read_data, 5));
  EXPECT_EQ("56789", read_data);
}

TEST_F(DebugDaemonLogSourceTest, ReadEndOfFileMedium) {
  std::string test_data = base::RandBytesAsString(10000);  // 10KB data
  std::string read_data;

  const size_t test_size = test_data.length();

  base::FilePath file_path = temp_dir_.GetPath().Append("test_med.txt");

  WriteFile(file_path, test_data.data(), test_size);

  read_data.clear();
  EXPECT_TRUE(ReadEndOfFile(file_path, &read_data, 15000));
  EXPECT_EQ(test_data, read_data);

  read_data.clear();
  EXPECT_TRUE(ReadEndOfFile(file_path, &read_data, 10000));
  EXPECT_EQ(test_data, read_data);

  read_data.clear();
  EXPECT_TRUE(ReadEndOfFile(file_path, &read_data, 1000));
  EXPECT_EQ(test_data.substr(test_size - 1000, 1000), read_data);

  read_data.clear();
  EXPECT_TRUE(ReadEndOfFile(file_path, &read_data, 300));
  EXPECT_EQ(test_data.substr(test_size - 300, 300), read_data);

  read_data.clear();
  EXPECT_TRUE(ReadEndOfFile(file_path, &read_data, 175));
  EXPECT_EQ(test_data.substr(test_size - 175, 175), read_data);
}

}  // namespace system_logs
