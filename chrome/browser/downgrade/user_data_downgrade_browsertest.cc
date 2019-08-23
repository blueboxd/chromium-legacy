// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/downgrade/user_data_downgrade.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool/thread_pool.h"
#include "base/test/test_reg_util_win.h"
#include "base/threading/thread_restrictions.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/install_static/install_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"

namespace downgrade {

class UserDataDowngradeBrowserTestBase : public InProcessBrowserTest {
 protected:
  // DeleteMovedUserDataSoon() is called after the test is run, and will post a
  // task to perform the actual deletion. Make sure this task gets a chance to
  // run, so that the check in TearDownInProcessBrowserTestFixture() works.
  class RunAllPendingTasksPostMainMessageLoopRunExtraParts
      : public ChromeBrowserMainExtraParts {
   public:
    void PostMainMessageLoopRun() override { content::RunAllTasksUntilIdle(); }
  };

  void CreatedBrowserMainParts(content::BrowserMainParts* parts) override {
    InProcessBrowserTest::CreatedBrowserMainParts(parts);
    static_cast<ChromeBrowserMainParts*>(parts)->AddParts(
        new RunAllPendingTasksPostMainMessageLoopRunExtraParts());
  }

  // content::BrowserTestBase:
  void SetUpInProcessBrowserTestFixture() override {
    HKEY root = HKEY_CURRENT_USER;
    ASSERT_NO_FATAL_FAILURE(registry_override_manager_.OverrideRegistry(root));
    key_.Create(root,
                install_static::GetClientStateKeyPath().c_str(),
                KEY_SET_VALUE | KEY_WOW64_32KEY);
  }

  // InProcessBrowserTest:
  bool SetUpUserDataDirectory() override {
    if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir_))
      return false;
    if (!CreateTemporaryFileInDir(user_data_dir_, &other_file_))
      return false;
    last_version_file_path_ = user_data_dir_.Append(kDowngradeLastVersionFile);
    std::string last_version = GetNextChromeVersion();
    base::WriteFile(last_version_file_path_, last_version.c_str(),
                    last_version.size());

    moved_user_data_dir_ = user_data_dir_.AddExtension(kDowngradeDeleteSuffix);

    return true;
  }

  // content::BrowserTestBase:
  // Verify the renamed user data directory has been deleted.
  void TearDownInProcessBrowserTestFixture() override {
    ASSERT_FALSE(base::DirectoryExists(moved_user_data_dir_));
    key_.Close();
  }

  std::string GetNextChromeVersion() {
    return base::Version(std::string(chrome::kChromeVersion) + "1").GetString();
  }

  base::FilePath last_version_file_path_;
  // The path to an arbitrary file in the user data dir that will be present
  // only when a reset does not take place.
  base::FilePath other_file_;
  base::FilePath user_data_dir_;
  base::FilePath moved_user_data_dir_;
  base::win::RegKey key_;
  registry_util::RegistryOverrideManager registry_override_manager_;
};

class UserDataDowngradeBrowserCopyAndCleanTest
    : public UserDataDowngradeBrowserTestBase {
 protected:
  // content::BrowserTestBase:
  void SetUpInProcessBrowserTestFixture() override {
    UserDataDowngradeBrowserTestBase::SetUpInProcessBrowserTestFixture();
    key_.WriteValue(L"DowngradeVersion",
                    base::ASCIIToUTF16(GetNextChromeVersion()).c_str());
  }

  // InProcessBrowserTest:
  // Verify the content of renamed user data directory.
  void SetUpOnMainThread() override {
    ASSERT_TRUE(base::DirectoryExists(moved_user_data_dir_));
    ASSERT_TRUE(
        base::PathExists(moved_user_data_dir_.Append(other_file_.BaseName())));
    EXPECT_EQ(GetNextChromeVersion(),
              GetLastVersion(moved_user_data_dir_).GetString());
    UserDataDowngradeBrowserTestBase::SetUpOnMainThread();
  }
};

class UserDataDowngradeBrowserNoResetTest
    : public UserDataDowngradeBrowserTestBase {
};

// Verify the user data directory has been renamed and created again after
// downgrade.
IN_PROC_BROWSER_TEST_F(UserDataDowngradeBrowserCopyAndCleanTest, Test) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ThreadPoolInstance::Get()->FlushForTesting();
  EXPECT_EQ(chrome::kChromeVersion, GetLastVersion(user_data_dir_).GetString());
  ASSERT_FALSE(base::PathExists(other_file_));
}

// Verify the user data directory will not be reset without downgrade.
IN_PROC_BROWSER_TEST_F(UserDataDowngradeBrowserNoResetTest, Test) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_EQ(chrome::kChromeVersion, GetLastVersion(user_data_dir_).GetString());
  ASSERT_TRUE(base::PathExists(other_file_));
}

}  // namespace downgrade
