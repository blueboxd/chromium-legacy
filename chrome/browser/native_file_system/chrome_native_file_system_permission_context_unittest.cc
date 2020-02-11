// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/native_file_system/chrome_native_file_system_permission_context.h"

#include <memory>
#include <string>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_path_override.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using content::BrowserContext;
using UserAction = ChromeNativeFileSystemPermissionContext::UserAction;
using PermissionStatus =
    content::NativeFileSystemPermissionGrant::PermissionStatus;
using PermissionRequestOutcome =
    content::NativeFileSystemPermissionGrant::PermissionRequestOutcome;
using SensitiveDirectoryResult =
    ChromeNativeFileSystemPermissionContext::SensitiveDirectoryResult;

class TestNativeFileSystemPermissionContext
    : public ChromeNativeFileSystemPermissionContext {
 public:
  explicit TestNativeFileSystemPermissionContext(
      content::BrowserContext* context)
      : ChromeNativeFileSystemPermissionContext(context) {}
  ~TestNativeFileSystemPermissionContext() override = default;

  // content::NativeFileSystemPermissionContext:
  scoped_refptr<content::NativeFileSystemPermissionGrant>
  GetReadPermissionGrant(const url::Origin& origin,
                         const base::FilePath& path,
                         bool is_directory,
                         int process_id,
                         int frame_id,
                         UserAction user_action) override {
    NOTREACHED();
    return nullptr;
  }
  scoped_refptr<content::NativeFileSystemPermissionGrant>
  GetWritePermissionGrant(const url::Origin& origin,
                          const base::FilePath& path,
                          bool is_directory,
                          int process_id,
                          int frame_id,
                          UserAction user_action) override {
    NOTREACHED();
    return nullptr;
  }

  // ChromeNativeFileSystemPermissionContext:
  Grants GetPermissionGrants(const url::Origin& origin,
                             int process_id,
                             int frame_id) override {
    NOTREACHED();
    return {};
  }
  void RevokeGrants(const url::Origin& origin,
                    int process_id,
                    int frame_id) override {
    NOTREACHED();
  }

 private:
  base::WeakPtr<ChromeNativeFileSystemPermissionContext> GetWeakPtr() override {
    return weak_factory_.GetWeakPtr();
  }

  base::WeakPtrFactory<TestNativeFileSystemPermissionContext> weak_factory_{
      this};
};

class ChromeNativeFileSystemPermissionContextTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    permission_context_ =
        std::make_unique<TestNativeFileSystemPermissionContext>(
            browser_context());
  }

  void TearDown() override {
    ASSERT_TRUE(temp_dir_.Delete());
  }

  SensitiveDirectoryResult ConfirmSensitiveDirectoryAccessSync(
      ChromeNativeFileSystemPermissionContext* context,
      const std::vector<base::FilePath>& paths) {
    base::RunLoop loop;
    SensitiveDirectoryResult out_result;
    permission_context_->ConfirmSensitiveDirectoryAccess(
        kTestOrigin, paths, /*is_directory=*/false, /*process_id=*/0,
        /*frame_id=*/0,
        base::BindLambdaForTesting([&](SensitiveDirectoryResult result) {
          out_result = result;
          loop.Quit();
        }));
    loop.Run();
    return out_result;
  }

  void SetDefaultContentSettingValue(ContentSettingsType type,
                                     ContentSetting value) {
    HostContentSettingsMap* content_settings =
        HostContentSettingsMapFactory::GetForProfile(&profile_);
    content_settings->SetDefaultContentSetting(type, value);
  }

  void SetContentSettingValueForOrigin(url::Origin origin,
                                       ContentSettingsType type,
                                       ContentSetting value) {
    HostContentSettingsMap* content_settings =
        HostContentSettingsMapFactory::GetForProfile(&profile_);
    content_settings->SetContentSettingDefaultScope(
        origin.GetURL(), origin.GetURL(), type,
        /*resource_identifier=*/std::string(), value);
  }

  ChromeNativeFileSystemPermissionContext* permission_context() {
    return permission_context_.get();
  }
  BrowserContext* browser_context() { return &profile_; }

 protected:
  const url::Origin kTestOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin kTestOrigin2 =
      url::Origin::Create(GURL("https://test.com"));
  const base::FilePath kTestPath =
      base::FilePath(FILE_PATH_LITERAL("/foo/bar"));
  const url::Origin kChromeOrigin = url::Origin::Create(GURL("chrome://test"));

  content::BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<ChromeNativeFileSystemPermissionContext> permission_context_;
  TestingProfile profile_;
};

#if !defined(OS_ANDROID)

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       ConfirmSensitiveDirectoryAccess_NoSpecialPath) {
  const base::FilePath kTestPath =
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
      base::FilePath(FILE_PATH_LITERAL("c:\\foo\\bar"));
#else
      base::FilePath(FILE_PATH_LITERAL("/foo/bar"));
#endif

  // Path outside any special directories should be allowed.
  EXPECT_EQ(
      SensitiveDirectoryResult::kAllowed,
      ConfirmSensitiveDirectoryAccessSync(permission_context(), {kTestPath}));

  // Empty set of paths should also be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAllowed,
            ConfirmSensitiveDirectoryAccessSync(permission_context(), {}));
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       ConfirmSensitiveDirectoryAccess_DontBlockAllChildren) {
  base::FilePath home_dir = temp_dir_.GetPath().AppendASCII("home");
  base::ScopedPathOverride home_override(base::DIR_HOME, home_dir, true, true);

  // Home directory itself should not be allowed.
  EXPECT_EQ(
      SensitiveDirectoryResult::kAbort,
      ConfirmSensitiveDirectoryAccessSync(permission_context(), {home_dir}));
  // Parent of home directory should also not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(permission_context(),
                                                {temp_dir_.GetPath()}));
  // Paths inside home directory should be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAllowed,
            ConfirmSensitiveDirectoryAccessSync(permission_context(),
                                                {home_dir.AppendASCII("foo")}));
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       ConfirmSensitiveDirectoryAccess_BlockAllChildren) {
  base::FilePath app_dir = temp_dir_.GetPath().AppendASCII("app");
  base::ScopedPathOverride app_override(chrome::DIR_APP, app_dir, true, true);

  // App directory itself should not be allowed.
  EXPECT_EQ(
      SensitiveDirectoryResult::kAbort,
      ConfirmSensitiveDirectoryAccessSync(permission_context(), {app_dir}));
  // Parent of App directory should also not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(permission_context(),
                                                {temp_dir_.GetPath()}));
  // Paths inside App directory should also not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(permission_context(),
                                                {app_dir.AppendASCII("foo")}));
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       ConfirmSensitiveDirectoryAccess_BlockChildrenNested) {
  base::FilePath user_data_dir = temp_dir_.GetPath().AppendASCII("user");
  base::ScopedPathOverride user_data_override(chrome::DIR_USER_DATA,
                                              user_data_dir, true, true);
  base::FilePath download_dir = user_data_dir.AppendASCII("downloads");
  base::ScopedPathOverride download_override(chrome::DIR_DEFAULT_DOWNLOADS,
                                             download_dir, true, true);

  // User Data directory itself should not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(permission_context(),
                                                {user_data_dir}));
  // Parent of User Data directory should also not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(permission_context(),
                                                {temp_dir_.GetPath()}));
  // The nested Download directory itself should not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(permission_context(),
                                                {download_dir}));
  // Paths inside the nested Download directory should be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAllowed,
            ConfirmSensitiveDirectoryAccessSync(
                permission_context(), {download_dir.AppendASCII("foo")}));
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       ConfirmSensitiveDirectoryAccess_RelativePathBlock) {
  base::FilePath home_dir = temp_dir_.GetPath().AppendASCII("home");
  base::ScopedPathOverride home_override(base::DIR_HOME, home_dir, true, true);

  // ~/.ssh should be blocked
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(
                permission_context(), {home_dir.AppendASCII(".ssh")}));
  // And anything inside ~/.ssh should also be blocked
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(
                permission_context(), {home_dir.AppendASCII(".ssh/id_rsa")}));
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       ConfirmSensitiveDirectoryAccess_ExplicitPathBlock) {
// Linux is the only OS where we have some blocked directories with explicit
// paths (as opposed to PathService provided paths).
#if defined(OS_LINUX)
  // /dev should be blocked.
  EXPECT_EQ(
      SensitiveDirectoryResult::kAbort,
      ConfirmSensitiveDirectoryAccessSync(
          permission_context(), {base::FilePath(FILE_PATH_LITERAL("/dev"))}));
  // As well as children of /dev.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(
                permission_context(),
                {base::FilePath(FILE_PATH_LITERAL("/dev/foo"))}));
#endif
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       CanObtainWritePermission_ContentSettingAsk) {
  SetDefaultContentSettingValue(
      ContentSettingsType::NATIVE_FILE_SYSTEM_WRITE_GUARD, CONTENT_SETTING_ASK);
  EXPECT_TRUE(permission_context()->CanObtainWritePermission(kTestOrigin));
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       CanObtainWritePermission_ContentSettingsBlock) {
  SetDefaultContentSettingValue(
      ContentSettingsType::NATIVE_FILE_SYSTEM_WRITE_GUARD,
      CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(permission_context()->CanObtainWritePermission(kTestOrigin));
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       CanObtainWritePermission_ContentSettingAllow) {
  // Note, chrome:// scheme is whitelisted. But we can't set default content
  // setting here because ALLOW is not an acceptable option.
  EXPECT_TRUE(permission_context()->CanObtainWritePermission(kChromeOrigin));
}

#endif  // !defined(OS_ANDROID)
