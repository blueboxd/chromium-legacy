// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_file_system/native_file_system_manager_impl.h"

#include "base/files/scoped_temp_dir.h"
#include "base/test/bind_test_util.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "content/browser/native_file_system/fixed_native_file_system_permission_grant.h"
#include "content/browser/native_file_system/mock_native_file_system_permission_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace content {

using base::test::RunOnceCallback;
using blink::mojom::PermissionStatus;
using storage::FileSystemURL;

class NativeFileSystemManagerImplTest : public testing::Test {
 public:
  NativeFileSystemManagerImplTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO) {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kNativeFileSystemAPI);
  }

  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    ASSERT_TRUE(dir_.GetPath().IsAbsolute());
    file_system_context_ = CreateFileSystemContextForTesting(
        /*quota_manager_proxy=*/nullptr, dir_.GetPath());

    chrome_blob_context_ = base::MakeRefCounted<ChromeBlobStorageContext>();
    chrome_blob_context_->InitializeOnIOThread(base::FilePath(), nullptr);

    manager_ = base::MakeRefCounted<NativeFileSystemManagerImpl>(
        file_system_context_, chrome_blob_context_, &permission_context_);

    manager_->BindRequest(kBindingContext, mojo::MakeRequest(&manager_ptr_));
  }

  template <typename HandleType>
  PermissionStatus GetPermissionStatusSync(bool writable, HandleType* handle) {
    PermissionStatus result;
    base::RunLoop loop;
    handle->GetPermissionStatus(
        writable, base::BindLambdaForTesting([&](PermissionStatus status) {
          result = status;
          loop.Quit();
        }));
    loop.Run();
    return result;
  }

 protected:
  const url::Origin kTestOrigin =
      url::Origin::Create(GURL("https://example.com"));
  const int kProcessId = 1;
  const int kFrameId = 2;
  const NativeFileSystemManagerImpl::BindingContext kBindingContext = {
      kTestOrigin, kProcessId, kFrameId};

  base::test::ScopedFeatureList scoped_feature_list_;
  TestBrowserThreadBundle scoped_task_environment_;

  base::ScopedTempDir dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_refptr<ChromeBlobStorageContext> chrome_blob_context_;

  testing::StrictMock<MockNativeFileSystemPermissionContext>
      permission_context_;
  scoped_refptr<NativeFileSystemManagerImpl> manager_;
  blink::mojom::NativeFileSystemManagerPtr manager_ptr_;

  scoped_refptr<FixedNativeFileSystemPermissionGrant> ask_grant_ =
      base::MakeRefCounted<FixedNativeFileSystemPermissionGrant>(
          FixedNativeFileSystemPermissionGrant::PermissionStatus::ASK);
  scoped_refptr<FixedNativeFileSystemPermissionGrant> allow_grant_ =
      base::MakeRefCounted<FixedNativeFileSystemPermissionGrant>(
          FixedNativeFileSystemPermissionGrant::PermissionStatus::GRANTED);
};

TEST_F(NativeFileSystemManagerImplTest, GetSandboxedFileSystem_Permissions) {
  blink::mojom::NativeFileSystemDirectoryHandlePtr root;
  base::RunLoop loop;
  manager_ptr_->GetSandboxedFileSystem(base::BindLambdaForTesting(
      [&](blink::mojom::NativeFileSystemErrorPtr result,
          blink::mojom::NativeFileSystemDirectoryHandlePtr handle) {
        EXPECT_EQ(base::File::FILE_OK, result->error_code);
        root = std::move(handle);
        loop.Quit();
      }));
  loop.Run();
  ASSERT_TRUE(root);
  EXPECT_EQ(PermissionStatus::GRANTED,
            GetPermissionStatusSync(/*writable=*/false, root.get()));
  EXPECT_EQ(PermissionStatus::GRANTED,
            GetPermissionStatusSync(/*writable=*/true, root.get()));
}

TEST_F(NativeFileSystemManagerImplTest, CreateFileEntryFromPath_Permissions) {
  const base::FilePath kTestPath(dir_.GetPath().AppendASCII("foo"));

  EXPECT_CALL(
      permission_context_,
      GetReadPermissionGrant(kTestOrigin, kTestPath, /*is_directory=*/false,
                             kProcessId, kFrameId))
      .WillOnce(testing::Return(allow_grant_));
  EXPECT_CALL(
      permission_context_,
      GetWritePermissionGrant(
          kTestOrigin, kTestPath, /*is_directory=*/false, kProcessId, kFrameId,
          NativeFileSystemPermissionContext::UserAction::kOpen))
      .WillOnce(testing::Return(ask_grant_));

  blink::mojom::NativeFileSystemEntryPtr entry =
      manager_->CreateFileEntryFromPath(kBindingContext, kTestPath);
  blink::mojom::NativeFileSystemFileHandlePtr handle(
      std::move(entry->entry_handle->get_file()));

  EXPECT_EQ(PermissionStatus::GRANTED,
            GetPermissionStatusSync(/*writable=*/false, handle.get()));
  EXPECT_EQ(PermissionStatus::ASK,
            GetPermissionStatusSync(/*writable=*/true, handle.get()));
}

TEST_F(NativeFileSystemManagerImplTest,
       CreateWritableFileEntryFromPath_Permissions) {
  const base::FilePath kTestPath(dir_.GetPath().AppendASCII("foo"));

  EXPECT_CALL(
      permission_context_,
      GetReadPermissionGrant(kTestOrigin, kTestPath, /*is_directory=*/false,
                             kProcessId, kFrameId))
      .WillOnce(testing::Return(allow_grant_));
  EXPECT_CALL(
      permission_context_,
      GetWritePermissionGrant(
          kTestOrigin, kTestPath, /*is_directory=*/false, kProcessId, kFrameId,
          NativeFileSystemPermissionContext::UserAction::kSave))
      .WillOnce(testing::Return(allow_grant_));

  blink::mojom::NativeFileSystemEntryPtr entry =
      manager_->CreateWritableFileEntryFromPath(kBindingContext, kTestPath);
  blink::mojom::NativeFileSystemFileHandlePtr handle(
      std::move(entry->entry_handle->get_file()));

  EXPECT_EQ(PermissionStatus::GRANTED,
            GetPermissionStatusSync(/*writable=*/false, handle.get()));
  EXPECT_EQ(PermissionStatus::GRANTED,
            GetPermissionStatusSync(/*writable=*/true, handle.get()));
}

TEST_F(NativeFileSystemManagerImplTest,
       CreateDirectoryEntryFromPath_Permissions) {
  const base::FilePath kTestPath(dir_.GetPath().AppendASCII("foo"));

  EXPECT_CALL(
      permission_context_,
      GetReadPermissionGrant(kTestOrigin, kTestPath, /*is_directory=*/true,
                             kProcessId, kFrameId))
      .WillOnce(testing::Return(allow_grant_));
  EXPECT_CALL(
      permission_context_,
      GetWritePermissionGrant(
          kTestOrigin, kTestPath, /*is_directory=*/true, kProcessId, kFrameId,
          NativeFileSystemPermissionContext::UserAction::kOpen))
      .WillOnce(testing::Return(ask_grant_));

  blink::mojom::NativeFileSystemEntryPtr entry =
      manager_->CreateDirectoryEntryFromPath(kBindingContext, kTestPath);
  blink::mojom::NativeFileSystemDirectoryHandlePtr handle(
      std::move(entry->entry_handle->get_directory()));
  EXPECT_EQ(PermissionStatus::GRANTED,
            GetPermissionStatusSync(/*writable=*/false, handle.get()));
  EXPECT_EQ(PermissionStatus::ASK,
            GetPermissionStatusSync(/*writable=*/true, handle.get()));
}

}  // namespace content
