// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/session/file_system_status.h"

#include <string.h>

#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

class FileSystemStatusTest : public testing::Test {
 public:
  FileSystemStatusTest() = default;
  ~FileSystemStatusTest() override = default;

  FileSystemStatusTest(const FileSystemStatusTest&) = delete;
  FileSystemStatusTest& operator=(const FileSystemStatusTest&) = delete;

  void SetUp() override { ASSERT_TRUE(dir_.CreateUniqueTempDir()); }

 protected:
  static bool IsAndroidDebuggable(const base::FilePath& json_path) {
    return FileSystemStatus::IsAndroidDebuggableForTesting(json_path);
  }
  static bool IsSystemImageExtFormat(const base::FilePath& path) {
    return FileSystemStatus::IsSystemImageExtFormatForTesting(path);
  }

  const base::FilePath& GetTempDir() const { return dir_.GetPath(); }

 private:
  base::ScopedTempDir dir_;
};

// Tests if androidboot.debuggable is set properly.
TEST_F(FileSystemStatusTest, IsAndroidDebuggable) {
  constexpr const char kAndroidDebuggableTrueJson[] = R"json({
    "ANDROID_DEBUGGABLE": true
  })json";
  constexpr const char kAndroidDebuggableFalseJson[] = R"json({
    "ANDROID_DEBUGGABLE": false
  })json";
  constexpr const char kInvalidTypeJson[] = R"json([
    42
  ])json";
  constexpr const char kInvalidJson[] = R"json({
    "ANDROID_DEBUGGABLE": true,
  })json";
  constexpr const char kKeyNotFoundJson[] = R"json({
    "BADKEY": "a"
  })json";
  constexpr const char kNonBooleanValue[] = R"json({
    "ANDROID_DEBUGGABLE": "a"
  })json";
  constexpr const char kBadKeyType[] = R"json({
    42: true
  })json";

  auto test = [](const base::FilePath& dir, const std::string& str) {
    base::FilePath path;
    if (!CreateTemporaryFileInDir(dir, &path))
      return false;
    base::WriteFile(path, str.data(), str.size());
    return IsAndroidDebuggable(path);
  };

  EXPECT_TRUE(test(GetTempDir(), kAndroidDebuggableTrueJson));
  EXPECT_FALSE(test(GetTempDir(), kAndroidDebuggableFalseJson));
  EXPECT_FALSE(test(GetTempDir(), kInvalidTypeJson));
  EXPECT_FALSE(test(GetTempDir(), kInvalidJson));
  EXPECT_FALSE(test(GetTempDir(), kKeyNotFoundJson));
  EXPECT_FALSE(test(GetTempDir(), kNonBooleanValue));
  EXPECT_FALSE(test(GetTempDir(), kBadKeyType));
}

// Tests the case where the json file doesn't exist.
TEST_F(FileSystemStatusTest, IsAndroidDebuggable_NonExistent) {
  EXPECT_FALSE(IsAndroidDebuggable(base::FilePath("/nonexistent-path")));
}

// Tests the case where the json file is not readable.
TEST_F(FileSystemStatusTest, IsAndroidDebuggable_CannotRead) {
  constexpr const char kValidJson[] = R"json({
    "ANDROID_DEBUGGABLE": true
  })json";
  base::FilePath path;
  ASSERT_TRUE(CreateTemporaryFileInDir(GetTempDir(), &path));
  base::WriteFile(path, kValidJson, strlen(kValidJson));
  base::SetPosixFilePermissions(path, 0300);  // not readable
  EXPECT_FALSE(IsAndroidDebuggable(path));
}

TEST_F(FileSystemStatusTest, IsSystemImageExtFormat_FileMissing) {
  EXPECT_FALSE(IsSystemImageExtFormat(base::FilePath("/nonexistent")));
}

TEST_F(FileSystemStatusTest, IsSystemImageExtFormat_FileSizeTooSmall) {
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFile(&file));
  char data[100];
  memset(data, 0, sizeof(data));
  base::WriteFile(file, data, sizeof(data));

  EXPECT_FALSE(IsSystemImageExtFormat(file));
}

TEST_F(FileSystemStatusTest, IsSystemImageExtFormat_MagicNumberDoesNotMatch) {
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFile(&file));
  char data[2048];
  memset(data, 0, sizeof(data));
  base::WriteFile(file, data, sizeof(data));

  EXPECT_FALSE(IsSystemImageExtFormat(file));
}

TEST_F(FileSystemStatusTest, IsSystemImageExtFormat_MagicNumberMatches) {
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFile(&file));
  char data[2048];
  memset(data, 0, sizeof(data));
  // Magic signature (0xEF53) is in little-endian order.
  data[0x400 + 0x38] = 0x53;
  data[0x400 + 0x39] = 0xEF;
  base::WriteFile(file, data, sizeof(data));

  EXPECT_TRUE(IsSystemImageExtFormat(file));
}

}  // namespace
}  // namespace arc
