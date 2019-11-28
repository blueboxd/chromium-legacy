// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/roaming_profile_directory_deleter_win.h"

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "chrome/common/chrome_paths_internal.h"
#include "testing/gtest/include/gtest/gtest.h"

class DeleteRoamingUserDataDirectoryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Get the path to the faux AppData directory and make sure it's empty (it
    // had better be).
    ASSERT_TRUE(base::PathService::Get(base::DIR_APP_DATA, &app_data_dir_));
    ASSERT_TRUE(base::DirectoryExists(app_data_dir_));
    ASSERT_TRUE(base::IsDirectoryEmpty(app_data_dir_));
  }

  // Creates the roaming User Data directory within the faux AppData dir. |dir|,
  // if not null, is populated with the path to the created directory.
  void MakeRoamingUserDataDirectory(base::FilePath* dir) {
    roaming_user_data_dir_.emplace();
    ASSERT_TRUE(
        chrome::GetDefaultRoamingUserDataDirectory(&*roaming_user_data_dir_));
    ASSERT_TRUE(base::CreateDirectory(*roaming_user_data_dir_));
    if (dir)
      *dir = *roaming_user_data_dir_;
  }

  const base::FilePath& app_data_dir() const { return app_data_dir_; }

  void RunTasksUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedPathOverride app_data_override_{base::DIR_APP_DATA};
  base::FilePath app_data_dir_;
  base::Optional<base::FilePath> roaming_user_data_dir_;
};

// Tests that the roaming AppData directory is not touched when there is no
// User Data dir within it.
TEST_F(DeleteRoamingUserDataDirectoryTest, DoesNotExist) {
  DeleteRoamingUserDataDirectoryLater();
  RunTasksUntilIdle();
  ASSERT_TRUE(base::DirectoryExists(app_data_dir()));
  ASSERT_TRUE(base::IsDirectoryEmpty(app_data_dir()));
}

// Tests that an empty User Data directory and empty intermediate directories
// are deleted.
TEST_F(DeleteRoamingUserDataDirectoryTest, EmptyUserData) {
  ASSERT_NO_FATAL_FAILURE(MakeRoamingUserDataDirectory(nullptr));
  DeleteRoamingUserDataDirectoryLater();
  RunTasksUntilIdle();
  ASSERT_TRUE(base::DirectoryExists(app_data_dir()));
  ASSERT_TRUE(base::IsDirectoryEmpty(app_data_dir()));
}

// Tests that a non-empty User Data directory is not touched.
TEST_F(DeleteRoamingUserDataDirectoryTest, NonEmptyUserData) {
  base::FilePath roaming_user_data_dir;
  ASSERT_NO_FATAL_FAILURE(MakeRoamingUserDataDirectory(&roaming_user_data_dir));
  ASSERT_EQ(
      base::WriteFile(
          roaming_user_data_dir.Append(FILE_PATH_LITERAL("file.txt")), "hi", 2),
      2);
  DeleteRoamingUserDataDirectoryLater();
  RunTasksUntilIdle();
  ASSERT_TRUE(base::DirectoryExists(roaming_user_data_dir));
  ASSERT_FALSE(base::IsDirectoryEmpty(roaming_user_data_dir));
}
