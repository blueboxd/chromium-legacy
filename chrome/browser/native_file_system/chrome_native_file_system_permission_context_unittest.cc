// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/native_file_system/chrome_native_file_system_permission_context.h"

#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using UserAction = ChromeNativeFileSystemPermissionContext::UserAction;
using PermissionStatus =
    content::NativeFileSystemPermissionGrant::PermissionStatus;

class ChromeNativeFileSystemPermissionContextTest : public testing::Test {
 protected:
  const url::Origin kTestOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const base::FilePath kTestPath =
      base::FilePath(FILE_PATH_LITERAL("/foo/bar"));
};

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       GetWritePermissionGrant_InitialState_OpenAction) {
  auto context =
      base::MakeRefCounted<ChromeNativeFileSystemPermissionContext>(nullptr);

  auto grant = context->GetWritePermissionGrant(
      kTestOrigin, kTestPath, /*is_directory=*/false, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       GetWritePermissionGrant_InitialState_WritableImplicitState) {
  auto context =
      base::MakeRefCounted<ChromeNativeFileSystemPermissionContext>(nullptr);

  auto grant = context->GetWritePermissionGrant(
      kTestOrigin, kTestPath, /*is_directory=*/false, UserAction::kSave);
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
}

TEST_F(ChromeNativeFileSystemPermissionContextTest,
       GetWritePermissionGrant_WriteGrantedChangesExistingGrant) {
  auto context =
      base::MakeRefCounted<ChromeNativeFileSystemPermissionContext>(nullptr);

  auto grant1 = context->GetWritePermissionGrant(
      kTestOrigin, kTestPath, /*is_directory=*/false, UserAction::kOpen);
  auto grant2 = context->GetWritePermissionGrant(
      kTestOrigin, kTestPath, /*is_directory=*/false, UserAction::kSave);
  auto grant3 = context->GetWritePermissionGrant(
      kTestOrigin, kTestPath, /*is_directory=*/false, UserAction::kOpen);
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
      kTestOrigin, kTestPath, /*is_directory=*/false, UserAction::kSave);
  EXPECT_EQ(PermissionStatus::GRANTED, grant->GetStatus());
  grant.reset();

  // After reset grant should go away, so new grant request should be in ASK
  // state.
  grant = context->GetWritePermissionGrant(
      kTestOrigin, kTestPath, /*is_directory=*/false, UserAction::kOpen);
  EXPECT_EQ(PermissionStatus::ASK, grant->GetStatus());
}
