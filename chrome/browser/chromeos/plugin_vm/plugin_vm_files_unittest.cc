// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_files.h"

#include "base/files/file_util.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/scoped_set_running_on_chromeos_for_testing.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plugin_vm {

using EnsureDefaultSharedDirsExistCallback =
    testing::StrictMock<base::MockCallback<
        base::OnceCallback<void(const base::FilePath& dir, bool result)>>>;

const char kLsbRelease[] =
    "CHROMEOS_RELEASE_NAME=Chrome OS\n"
    "CHROMEOS_RELEASE_VERSION=1.2.3.4\n";

class PluginVmFilesTest : public testing::Test {
 protected:
  base::FilePath GetMyFilesFolderPath() {
    return file_manager::util::GetMyFilesFolderForProfile(&profile_);
  }

  base::FilePath GetPvmDefaultPath() {
    return GetMyFilesFolderPath().Append("PvmDefault");
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  chromeos::ScopedSetRunningOnChromeOSForTesting fake_release_{kLsbRelease, {}};
};

TEST_F(PluginVmFilesTest, DirNotExists) {
  EnsureDefaultSharedDirsExistCallback callback;
  EnsureDefaultSharedDirsExist(&profile_, callback.Get());
  EXPECT_CALL(callback, Run(GetPvmDefaultPath(), true));
  task_environment_.RunUntilIdle();
}

TEST_F(PluginVmFilesTest, DirAlreadyExists) {
  EXPECT_TRUE(base::CreateDirectory(GetPvmDefaultPath()));

  EnsureDefaultSharedDirsExistCallback callback;
  EnsureDefaultSharedDirsExist(&profile_, callback.Get());
  EXPECT_CALL(callback, Run(GetPvmDefaultPath(), true));
  task_environment_.RunUntilIdle();
}

TEST_F(PluginVmFilesTest, FileAlreadyExists) {
  EXPECT_TRUE(base::CreateDirectory(GetMyFilesFolderPath()));
  EXPECT_TRUE(base::WriteFile(GetPvmDefaultPath(), ""));

  EnsureDefaultSharedDirsExistCallback callback;
  EnsureDefaultSharedDirsExist(&profile_, callback.Get());
  EXPECT_CALL(callback, Run(GetPvmDefaultPath(), false));
  task_environment_.RunUntilIdle();
}

TEST_F(PluginVmFilesTest, SubDirAlreadyExists) {
  base::FilePath root = GetPvmDefaultPath();
  base::FilePath sub_dir = root.Append("Documents");
  EXPECT_TRUE(base::CreateDirectory(sub_dir));

  EnsureDefaultSharedDirsExistCallback callback;
  EnsureDefaultSharedDirsExist(&profile_, callback.Get());
  EXPECT_CALL(callback, Run(GetPvmDefaultPath(), true));
  task_environment_.RunUntilIdle();
}

TEST_F(PluginVmFilesTest, SubDirFileAlreadyExists) {
  base::FilePath root = GetPvmDefaultPath();
  base::FilePath sub_dir = root.Append("Documents");
  EXPECT_TRUE(base::CreateDirectory(root));
  EXPECT_TRUE(base::WriteFile(sub_dir, ""));

  EnsureDefaultSharedDirsExistCallback callback;
  EnsureDefaultSharedDirsExist(&profile_, callback.Get());
  EXPECT_CALL(callback, Run(GetPvmDefaultPath(), true));
  task_environment_.RunUntilIdle();
}

}  // namespace plugin_vm
