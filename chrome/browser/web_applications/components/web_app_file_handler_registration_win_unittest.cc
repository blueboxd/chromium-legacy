// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_file_handler_registration_win.h"

#include <set>
#include <string>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_reg_util_win.h"
#include "base/test/test_timeouts.h"
#include "base/win/windows_version.h"
#include "chrome/browser/web_applications/components/web_app_shortcut_win.h"
#include "chrome/browser/web_applications/test/test_file_handler_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

class UpdateChromeExePathTest : public testing::Test {
 protected:
  UpdateChromeExePathTest() : user_data_dir_override_(chrome::DIR_USER_DATA) {}

  void SetUp() override {
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir_));
    ASSERT_FALSE(user_data_dir_.empty());
    last_browser_file_ = user_data_dir_.Append(kLastBrowserFile);
  }

  static base::FilePath GetCurrentExePath() {
    base::FilePath current_exe_path;
    EXPECT_TRUE(base::PathService::Get(base::FILE_EXE, &current_exe_path));
    return current_exe_path;
  }

  base::FilePath GetLastBrowserPathFromFile() const {
    std::string last_browser_file_data;
    EXPECT_TRUE(
        base::ReadFileToString(last_browser_file_, &last_browser_file_data));
    base::FilePath::StringPieceType last_browser_path(
        reinterpret_cast<const base::FilePath::CharType*>(
            last_browser_file_data.data()),
        last_browser_file_data.size() / sizeof(base::FilePath::CharType));
    return base::FilePath(last_browser_path);
  }

  const base::FilePath& user_data_dir() const { return user_data_dir_; }

 private:
  // Redirect |chrome::DIR_USER_DATA| to a temporary directory during testing.
  base::ScopedPathOverride user_data_dir_override_;

  base::FilePath user_data_dir_;
  base::FilePath last_browser_file_;
};

TEST_F(UpdateChromeExePathTest, UpdateChromeExePath) {
  UpdateChromeExePath(user_data_dir());
  EXPECT_EQ(GetLastBrowserPathFromFile(), GetCurrentExePath());
}

class WebAppFileHandlerRegistrationWinTest : public testing::Test {
 protected:
  WebAppFileHandlerRegistrationWinTest() {}

  void SetUp() override {
    // Set up fake windows registry
    ASSERT_NO_FATAL_FAILURE(
        registry_override_.OverrideRegistry(HKEY_LOCAL_MACHINE));
    ASSERT_NO_FATAL_FAILURE(
        registry_override_.OverrideRegistry(HKEY_CURRENT_USER));
    // Until the CL to create the PWA launcher is submitted, create it by
    // hand. TODO(davidbienvenu): Remove this once cl/1815220 lands.
    base::File pwa_launcher(GetChromePwaLauncherPath(),
                            base::File::FLAG_CREATE);
  }

  Profile* profile() { return &profile_; }

  // Returns true if Chrome extension with AppId |app_id| has its corresponding
  // prog_id registered in Windows registry to handle files with extension
  // |file_ext|, false otherwise.
  bool ProgIdRegisteredForFileExtension(const std::string& file_ext,
                                        const AppId& app_id) {
    base::string16 key_name(ShellUtil::kRegClasses);
    key_name.push_back(base::FilePath::kSeparators[0]);
    key_name.append(base::UTF8ToUTF16(file_ext));
    key_name.push_back(base::FilePath::kSeparators[0]);
    key_name.append(ShellUtil::kRegOpenWithProgids);
    base::win::RegKey key;
    std::wstring value;
    EXPECT_EQ(ERROR_SUCCESS,
              key.Open(HKEY_CURRENT_USER, key_name.c_str(), KEY_READ));
    base::string16 prog_id = GetProgIdForApp(profile(), app_id);
    return key.ReadValue(prog_id.c_str(), &value) == ERROR_SUCCESS &&
           value == L"";
  }

  // Creates a "Web Applications" directory containing a subdirectory for
  // |app_id| inside |profile|'s data directory, then returns the expected app-
  // launcher path inside the subdirectory for |app_id|.
  base::FilePath CreateDataDirectoryAndGetLauncherPathForApp(
      Profile* profile,
      const AppId app_id,
      const std::string& sanitized_app_name) {
    base::FilePath web_app_dir(
        GetWebAppDataDirectory(profile->GetPath(), app_id, GURL()));
    // Make sure web app dir exists. Normally installing an extension would
    // handle this.
    EXPECT_TRUE(base::CreateDirectory(web_app_dir));

    base::FilePath app_specific_launcher_filename(
        base::ASCIIToUTF16(sanitized_app_name));
    if (base::win::GetVersion() > base::win::Version::WIN7) {
      app_specific_launcher_filename =
          app_specific_launcher_filename.AddExtension(L"exe");
    }

    return web_app_dir.Append(app_specific_launcher_filename);
  }

 private:
  registry_util::RegistryOverrideManager registry_override_;
  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::IO_MAINLOOP};
  TestingProfile profile_;
};

// Test various attributes of ProgIds returned by GetAppIdForApp.
TEST_F(WebAppFileHandlerRegistrationWinTest, GetProgIdForApp) {
  // Create a long app_id and verify that the prog id is less
  // than 39 characters, and only contains alphanumeric characters and
  // non leading '.'s See
  // https://docs.microsoft.com/en-us/windows/win32/com/-progid--key.
  AppId app_id1("app_id12345678901234567890123456789012345678901234");
  constexpr unsigned int kMaxProgIdLen = 39;
  base::string16 prog_id1 = GetProgIdForApp(profile(), app_id1);
  EXPECT_LE(prog_id1.length(), kMaxProgIdLen);
  for (auto itr = prog_id1.begin(); itr != prog_id1.end(); itr++)
    EXPECT_TRUE(std::isalnum(*itr) || (*itr == '.' && itr != prog_id1.begin()));
  AppId app_id2("different_appid");
  // Check that different app ids in the same profile have different
  // prog ids.
  EXPECT_NE(prog_id1, GetProgIdForApp(profile(), app_id2));

  // Create a different profile, and verify that the prog id for the same
  // app_id in a different profile is different.
  TestingProfile profile2;
  EXPECT_NE(prog_id1, GetProgIdForApp(&profile2, app_id1));
}

TEST_F(WebAppFileHandlerRegistrationWinTest, RegisterFileHandlersForWebApp) {
  // Set up a test profile.
  std::set<std::string> file_extensions({"txt", "doc"});
  AppId app_id("app_id");
  std::string app_name("app name");
  base::FilePath app_specific_launcher_path =
      CreateDataDirectoryAndGetLauncherPathForApp(profile(), app_id, app_name);

  RegisterFileHandlersWithOs(app_id, app_name, profile(), file_extensions,
                             /*mime_types=*/{});
  base::ThreadPoolInstance::Get()->FlushForTesting();
  base::FilePath registered_app_path = ShellUtil::GetApplicationPathForProgId(
      GetProgIdForApp(profile(), app_id));
  EXPECT_FALSE(registered_app_path.empty());
  EXPECT_TRUE(base::PathExists(app_specific_launcher_path));
  EXPECT_EQ(app_specific_launcher_path, registered_app_path);

  // .txt should have |app_name| in its Open With list.
  EXPECT_TRUE(ProgIdRegisteredForFileExtension(".txt", app_id));
  EXPECT_TRUE(ProgIdRegisteredForFileExtension(".doc", app_id));
}

TEST_F(WebAppFileHandlerRegistrationWinTest, UnregisterFileHandlersForWebApp) {
  // Register file handlers, and then verify that unregistering removes
  // the registry settings and the app-specific launcher.
  std::set<std::string> file_extensions({"txt", "doc"});
  AppId app_id("app_id");
  std::string app_name("app name");
  base::FilePath app_specific_launcher_path =
      CreateDataDirectoryAndGetLauncherPathForApp(profile(), app_id, app_name);

  RegisterFileHandlersWithOs(app_id, app_name, profile(), file_extensions,
                             /*mime_types=*/{});
  base::ThreadPoolInstance::Get()->FlushForTesting();
  EXPECT_TRUE(base::PathExists(app_specific_launcher_path));
  EXPECT_TRUE(ProgIdRegisteredForFileExtension(".txt", app_id));
  EXPECT_TRUE(ProgIdRegisteredForFileExtension(".doc", app_id));

  UnregisterFileHandlersWithOs(app_id, profile());
  base::ThreadPoolInstance::Get()->FlushForTesting();
  EXPECT_FALSE(base::PathExists(app_specific_launcher_path));

  EXPECT_FALSE(ProgIdRegisteredForFileExtension(".txt", app_id));
  EXPECT_FALSE(ProgIdRegisteredForFileExtension(".doc", app_id));
}

// Test that invalid file name characters in app_name are replaced with '_'.
TEST_F(WebAppFileHandlerRegistrationWinTest, AppNameWithInvalidChars) {
  std::set<std::string> file_extensions({"txt"});
  AppId app_id("app_id");

  // '*' is an invalid char in Windows file names, so it should be replaced
  // with '_'.
  std::string app_name("app*name");
  base::FilePath app_specific_launcher_path =
      CreateDataDirectoryAndGetLauncherPathForApp(profile(), app_id,
                                                  "app_name");

  RegisterFileHandlersWithOs(app_id, app_name, profile(),
                             /*file_extensions*/ {"txt"}, /*mime_types=*/{});

  base::ThreadPoolInstance::Get()->FlushForTesting();
  base::FilePath registered_app_path = ShellUtil::GetApplicationPathForProgId(
      GetProgIdForApp(profile(), app_id));
  EXPECT_FALSE(registered_app_path.empty());
  EXPECT_TRUE(base::PathExists(app_specific_launcher_path));
  EXPECT_EQ(app_specific_launcher_path, registered_app_path);
}

// Test that an app name that is a reserved filename on Windows has '_' appended
// to it when used as a filename for its launcher.
TEST_F(WebAppFileHandlerRegistrationWinTest, AppNameIsReservedFilename) {
  std::set<std::string> file_extensions({"txt"});
  AppId app_id("app_id");

  // "con" is a reserved filename on Windows, so it should have '_' appended.
  std::string app_name("con");
  base::FilePath app_specific_launcher_path =
      CreateDataDirectoryAndGetLauncherPathForApp(profile(), app_id, "con_");

  RegisterFileHandlersWithOs(app_id, app_name, profile(),
                             /*file_extensions*/ {"txt"}, /*mime_types=*/{});

  base::ThreadPoolInstance::Get()->FlushForTesting();
  base::FilePath registered_app_path = ShellUtil::GetApplicationPathForProgId(
      GetProgIdForApp(profile(), app_id));
  EXPECT_FALSE(registered_app_path.empty());
  EXPECT_TRUE(base::PathExists(app_specific_launcher_path));
  EXPECT_EQ(app_specific_launcher_path, registered_app_path);
}

// Test that an app name that consists of a reserved filename on Windows plus
// '.' has all its '.' characters replaced by '_' when used as a filename for
// its launcher.
TEST_F(WebAppFileHandlerRegistrationWinTest, AppNameIsReservedFilenameWithDot) {
  std::set<std::string> file_extensions({"txt"});
  AppId app_id("app_id");

  // "con" is a reserved filename on Windows, and appending '_' won't make it
  // legitimate (because any text after '.' is interpreted as part of a file
  // extension), so all '.' characters should be replaced with '_'.
  std::string app_name("con.nect");
  base::FilePath app_specific_launcher_path =
      CreateDataDirectoryAndGetLauncherPathForApp(profile(), app_id,
                                                  "con_nect");

  RegisterFileHandlersWithOs(app_id, app_name, profile(),
                             /*file_extensions*/ {"txt"}, /*mime_types=*/{});

  base::ThreadPoolInstance::Get()->FlushForTesting();
  base::FilePath registered_app_path = ShellUtil::GetApplicationPathForProgId(
      GetProgIdForApp(profile(), app_id));
  EXPECT_FALSE(registered_app_path.empty());
  EXPECT_TRUE(base::PathExists(app_specific_launcher_path));
  EXPECT_EQ(app_specific_launcher_path, registered_app_path);
}

}  // namespace web_app
