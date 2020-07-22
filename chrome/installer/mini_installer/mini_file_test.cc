// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/mini_installer/mini_file.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/installer/mini_installer/path_string.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mini_installer {

// A test harness for MiniFile. The MiniFile::DeleteOnClose parameter is passed
// to the test instance's constructor.
class MiniFileTest : public ::testing::TestWithParam<MiniFile::DeleteOnClose> {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void TearDown() override { EXPECT_TRUE(temp_dir_.Delete()); }

  static MiniFile::DeleteOnClose delete_on_close() { return GetParam(); }

  static bool should_delete_on_close() {
    return delete_on_close() != MiniFile::DeleteOnClose::kNo;
  }

  const base::FilePath& temp_dir() const { return temp_dir_.GetPath(); }

 private:
  base::ScopedTempDir temp_dir_;
};

// Create should create a file.
TEST_P(MiniFileTest, Create) {
  const base::FilePath file_path = temp_dir().Append(FILE_PATH_LITERAL("HUM"));

  MiniFile file(delete_on_close());
  ASSERT_TRUE(file.Create(file_path.value().c_str()));
  EXPECT_TRUE(base::PathExists(file_path));
}

// Created files should be deletable by others and should vanish when closed.
TEST_P(MiniFileTest, CreateDeleteIsShared) {
  const base::FilePath file_path = temp_dir().Append(FILE_PATH_LITERAL("HUM"));

  MiniFile file(delete_on_close());
  ASSERT_TRUE(file.Create(file_path.value().c_str()));
  // DeleteFile uses POSIX semantics, so the file appears to vanish immediately.
  ASSERT_TRUE(base::DeleteFile(file_path));
  file.Close();
  EXPECT_FALSE(base::PathExists(file_path));
}

// Tests that a file can be opened without shared write access after write
// permissions are dropped.
TEST_P(MiniFileTest, DropWritePermission) {
  const base::FilePath file_path = temp_dir().Append(FILE_PATH_LITERAL("HUM"));

  MiniFile file(delete_on_close());
  ASSERT_TRUE(file.Create(file_path.value().c_str()));
  ASSERT_FALSE(base::File(file_path, base::File::FLAG_OPEN |
                                         base::File::FLAG_READ |
                                         base::File::FLAG_EXCLUSIVE_WRITE |
                                         base::File::FLAG_SHARE_DELETE)
                   .IsValid());
  ASSERT_TRUE(file.DropWritePermission());
  EXPECT_TRUE(base::File(file_path, base::File::FLAG_OPEN |
                                        base::File::FLAG_READ |
                                        base::File::FLAG_EXCLUSIVE_WRITE |
                                        base::File::FLAG_SHARE_DELETE)
                  .IsValid());
}

// Close should really close.
TEST_P(MiniFileTest, CreateThenClose) {
  const base::FilePath file_path = temp_dir().Append(FILE_PATH_LITERAL("HUM"));

  MiniFile file(delete_on_close());
  ASSERT_TRUE(file.Create(file_path.value().c_str()));
  file.Close();
  EXPECT_FALSE(file.IsValid());
  EXPECT_EQ(*file.path(), 0);

  ASSERT_NE(base::PathExists(file_path), should_delete_on_close());
  if (!should_delete_on_close()) {
    // If closing should not have deleted the file, it should now be possible to
    // open it with exclusive access.
    EXPECT_TRUE(base::File(file_path, base::File::FLAG_OPEN |
                                          base::File::FLAG_READ |
                                          base::File::FLAG_EXCLUSIVE_READ |
                                          base::File::FLAG_EXCLUSIVE_WRITE)
                    .IsValid());
  }
}

// DuplicateHandle should work as advertized.
TEST_P(MiniFileTest, DuplicateHandle) {
  const base::FilePath file_path = temp_dir().Append(FILE_PATH_LITERAL("HUM"));

  MiniFile file(delete_on_close());
  ASSERT_TRUE(file.Create(file_path.value().c_str()));
  HANDLE dup = file.DuplicateHandle();
  ASSERT_NE(dup, INVALID_HANDLE_VALUE);

  // Check that the two handles reference the same file.
  BY_HANDLE_FILE_INFORMATION info1 = {};
  ASSERT_NE(::GetFileInformationByHandle(file.GetHandleUnsafe(), &info1),
            FALSE);
  BY_HANDLE_FILE_INFORMATION info2 = {};
  ASSERT_NE(::GetFileInformationByHandle(dup, &info2), FALSE);
  EXPECT_EQ(info1.dwVolumeSerialNumber, info2.dwVolumeSerialNumber);
  EXPECT_EQ(info1.nFileIndexHigh, info2.nFileIndexHigh);
  EXPECT_EQ(info1.nFileIndexLow, info2.nFileIndexLow);

  ::CloseHandle(std::exchange(dup, INVALID_HANDLE_VALUE));
}

// Open should provide access to a file just like Create, but for a file that
// already exists.
TEST_P(MiniFileTest, Open) {
  const base::FilePath file_path = temp_dir().Append(FILE_PATH_LITERAL("HUM"));

  ASSERT_TRUE(
      base::File(file_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE)
          .IsValid());
  ASSERT_TRUE(base::PathExists(file_path));

  PathString path_string;
  ASSERT_TRUE(path_string.assign(file_path.value().c_str()));
  MiniFile file(delete_on_close());
  ASSERT_TRUE(file.Open(path_string));
}

// Close should really close.
TEST_P(MiniFileTest, OpenThenClose) {
  const base::FilePath file_path = temp_dir().Append(FILE_PATH_LITERAL("HUM"));

  ASSERT_TRUE(
      base::File(file_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE)
          .IsValid());
  ASSERT_TRUE(base::PathExists(file_path));

  MiniFile file(delete_on_close());
  PathString path_string;
  ASSERT_TRUE(path_string.assign(file_path.value().c_str()));
  ASSERT_TRUE(file.Open(path_string));
  file.Close();
  EXPECT_FALSE(file.IsValid());
  EXPECT_EQ(*file.path(), 0);

  ASSERT_NE(base::PathExists(file_path), should_delete_on_close());
  if (!should_delete_on_close()) {
    // If closing should not have deleted the file, it should now be possible to
    // open it with exclusive access.
    EXPECT_TRUE(base::File(file_path, base::File::FLAG_OPEN |
                                          base::File::FLAG_READ |
                                          base::File::FLAG_EXCLUSIVE_READ |
                                          base::File::FLAG_EXCLUSIVE_WRITE)
                    .IsValid());
  }
}

TEST_P(MiniFileTest, Path) {
  const base::FilePath file_path = temp_dir().Append(FILE_PATH_LITERAL("HUM"));

  MiniFile file(delete_on_close());
  EXPECT_EQ(*file.path(), 0);

  ASSERT_TRUE(file.Create(file_path.value().c_str()));
  EXPECT_STREQ(file.path(), file_path.value().c_str());

  file.Close();
  EXPECT_EQ(*file.path(), 0);
}

TEST_P(MiniFileTest, GetHandleUnsafe) {
  const base::FilePath file_path = temp_dir().Append(FILE_PATH_LITERAL("HUM"));

  MiniFile file(delete_on_close());
  EXPECT_EQ(file.GetHandleUnsafe(), INVALID_HANDLE_VALUE);

  ASSERT_TRUE(file.Create(file_path.value().c_str()));
  EXPECT_NE(file.GetHandleUnsafe(), INVALID_HANDLE_VALUE);

  file.Close();
  EXPECT_EQ(file.GetHandleUnsafe(), INVALID_HANDLE_VALUE);
}

INSTANTIATE_TEST_SUITE_P(DoNotDeleteOnClose,
                         MiniFileTest,
                         ::testing::Values(MiniFile::DeleteOnClose::kNo));
INSTANTIATE_TEST_SUITE_P(DeleteOnClose,
                         MiniFileTest,
                         ::testing::Values(MiniFile::DeleteOnClose::kYes));

}  // namespace mini_installer
