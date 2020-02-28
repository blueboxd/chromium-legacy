// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/updater/mac/installer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater_setup {

class ChromeUpdaterMacSetupTest : public ::testing::Test {
 public:
  ~ChromeUpdaterMacSetupTest() override = default;
};

TEST_F(ChromeUpdaterMacSetupTest, InstallFromDMG) {
  // Get the path to the test file.
  base::FilePath test_data_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
  test_data_dir = test_data_dir.Append(FILE_PATH_LITERAL("updater"));
  ASSERT_TRUE(base::PathExists(test_data_dir));
  // Get the DMG path and check that it exists.
  const base::FilePath dmg_file_path =
      test_data_dir.Append(FILE_PATH_LITERAL("updater_setup_test_dmg.dmg"));
  ASSERT_TRUE(base::PathExists(dmg_file_path));
  ASSERT_TRUE(updater::InstallFromDMG(dmg_file_path, ""));
}

TEST_F(ChromeUpdaterMacSetupTest, InstallFromDMGWithArgs) {
  // Get the path to the test file.
  base::FilePath test_data_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
  test_data_dir = test_data_dir.Append(FILE_PATH_LITERAL("updater"));
  ASSERT_TRUE(base::PathExists(test_data_dir));
  // Get the DMG path and check that it exists.
  const base::FilePath dmg_file_path =
      test_data_dir.Append(FILE_PATH_LITERAL("updater_setup_test_dmg.dmg"));
  ASSERT_TRUE(base::PathExists(dmg_file_path));
  ASSERT_TRUE(updater::InstallFromDMG(dmg_file_path, "-arg1 -arg2"));
}

}  // namespace updater_setup
