// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/native_file_system/chrome_native_file_system_permission_context.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_path_override.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using UserAction = ChromeNativeFileSystemPermissionContext::UserAction;
using PermissionStatus =
    content::NativeFileSystemPermissionGrant::PermissionStatus;
using SensitiveDirectoryResult =
    ChromeNativeFileSystemPermissionContext::SensitiveDirectoryResult;

class ChromeNativeFileSystemPermissionContextTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void TearDown() override { ASSERT_TRUE(temp_dir_.Delete()); }

  SensitiveDirectoryResult ConfirmSensitiveDirectoryAccessSync(
      ChromeNativeFileSystemPermissionContext* context,
      const std::vector<base::FilePath>& paths) {
    base::RunLoop loop;
    SensitiveDirectoryResult out_result;
    context->ConfirmSensitiveDirectoryAccess(
        kTestOrigin, paths, 0, 0,
        base::BindLambdaForTesting([&](SensitiveDirectoryResult result) {
          out_result = result;
          loop.Quit();
        }));
    loop.Run();
    return out_result;
  }

 protected:
  content::TestBrowserThreadBundle scoped_task_environment_;
  base::ScopedTempDir temp_dir_;

  const url::Origin kTestOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const base::FilePath kTestPath =
      base::FilePath(FILE_PATH_LITERAL("/foo/bar"));
  const int kProcessId = 1;
  const int kFrameId = 2;
};

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       GetWritePermissionGrant_InitialState_OpenAction) {
  auto context =
      base::MakeRefCounted<ChromeNativeFileSystemPermissionContext>(nullptr);

  auto grant = context->GetWritePermissionGrant(
      kTestOrigin, kTestPath,
      /*is_directory=*/false, kProcessId, kFrameId, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       GetWritePermissionGrant_InitialState_WritableImplicitState) {
  auto context =
      base::MakeRefCounted<ChromeNativeFileSystemPermissionContext>(nullptr);

  auto grant = context->GetWritePermissionGrant(
      kTestOrigin, kTestPath, /*is_directory=*/false, kProcessId, kFrameId,
      UserAction::kSave);
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       GetWritePermissionGrant_WriteGrantedChangesExistingGrant) {
  auto context =
      base::MakeRefCounted<ChromeNativeFileSystemPermissionContext>(nullptr);

  auto grant1 = context->GetWritePermissionGrant(
      kTestOrigin, kTestPath, /*is_directory=*/false, kProcessId, kFrameId,
      UserAction::kOpen);
  auto grant2 = context->GetWritePermissionGrant(
      kTestOrigin, kTestPath, /*is_directory=*/false, kProcessId, kFrameId,
      UserAction::kSave);
  auto grant3 = context->GetWritePermissionGrant(
      kTestOrigin, kTestPath, /*is_directory=*/false, kProcessId, kFrameId,
      UserAction::kOpen);
  // All grants should be the same grant, and be granted.
  EXPECT_EQ(grant1, grant2);
  EXPECT_EQ(grant1, grant3);
  EXPECT_EQ(PermissionStatus::GRANTED, grant1->GetStatus());
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       GetWritePermissionGrant_GrantIsRevokedWhenNoLongerUsed) {
  auto context =
      base::MakeRefCounted<ChromeNativeFileSystemPermissionContext>(nullptr);

  auto grant = context->GetWritePermissionGrant(
      kTestOrigin, kTestPath, /*is_directory=*/false, kProcessId, kFrameId,
      UserAction::kSave);
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
  grant.reset();

  // After reset grant should go away, so new grant request should be in ASK
  // state.
  grant = context->GetWritePermissionGrant(kTestOrigin, kTestPath,
                                           /*is_directory=*/false, kProcessId,
                                           kFrameId, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       ConfirmSensitiveDirectoryAccess_NoSpecialPath) {
  auto context =
      base::MakeRefCounted<ChromeNativeFileSystemPermissionContext>(nullptr);

  const base::FilePath kTestPath =
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
      base::FilePath(FILE_PATH_LITERAL("c:\\foo\\bar"));
#else
      base::FilePath(FILE_PATH_LITERAL("/foo/bar"));
#endif

  // Path outside any special directories should be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAllowed,
            ConfirmSensitiveDirectoryAccessSync(context.get(), {kTestPath}));

  // Empty set of paths should also be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAllowed,
            ConfirmSensitiveDirectoryAccessSync(context.get(), {}));
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       ConfirmSensitiveDirectoryAccess_DontBlockAllChildren) {
  auto context =
      base::MakeRefCounted<ChromeNativeFileSystemPermissionContext>(nullptr);

  base::FilePath home_dir = temp_dir_.GetPath().AppendASCII("home");
  base::ScopedPathOverride home_override(base::DIR_HOME, home_dir, true, true);

  // Home directory itself should not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(context.get(), {home_dir}));
  // Parent of home directory should also not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(context.get(),
                                                {temp_dir_.GetPath()}));
  // Paths inside home directory should be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAllowed,
            ConfirmSensitiveDirectoryAccessSync(context.get(),
                                                {home_dir.AppendASCII("foo")}));
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       ConfirmSensitiveDirectoryAccess_BlockAllChildren) {
  auto context =
      base::MakeRefCounted<ChromeNativeFileSystemPermissionContext>(nullptr);

  base::FilePath app_dir = temp_dir_.GetPath().AppendASCII("app");
  base::ScopedPathOverride app_override(chrome::DIR_APP, app_dir, true, true);

  // App directory itself should not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(context.get(), {app_dir}));
  // Parent of App directory should also not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(context.get(),
                                                {temp_dir_.GetPath()}));
  // Paths inside App directory should also not be allowed.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(context.get(),
                                                {app_dir.AppendASCII("foo")}));
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       ConfirmSensitiveDirectoryAccess_RelativePathBlock) {
  auto context =
      base::MakeRefCounted<ChromeNativeFileSystemPermissionContext>(nullptr);

  base::FilePath home_dir = temp_dir_.GetPath().AppendASCII("home");
  base::ScopedPathOverride home_override(base::DIR_HOME, home_dir, true, true);

  // ~/.ssh should be blocked
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(
                context.get(), {home_dir.AppendASCII(".ssh")}));
  // And anything inside ~/.ssh should also be blocked
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(
                context.get(), {home_dir.AppendASCII(".ssh/id_rsa")}));
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       ConfirmSensitiveDirectoryAccess_ExplicitPathBlock) {
// Linux is the only OS where we have some blocked directories with explicit
// paths (as opposed to PathService provided paths).
#if defined(OS_LINUX)
  auto context =
      base::MakeRefCounted<ChromeNativeFileSystemPermissionContext>(nullptr);

  // /dev should be blocked.
  EXPECT_EQ(SensitiveDirectoryResult::kAbort,
            ConfirmSensitiveDirectoryAccessSync(
                context.get(), {base::FilePath(FILE_PATH_LITERAL("/dev"))}));
  // As well as children of /dev.
  EXPECT_EQ(
      SensitiveDirectoryResult::kAbort,
      ConfirmSensitiveDirectoryAccessSync(
          context.get(), {base::FilePath(FILE_PATH_LITERAL("/dev/foo"))}));
#endif
}
