// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_file_system/native_file_system_manager_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "content/browser/native_file_system/fixed_native_file_system_permission_grant.h"
#include "content/browser/native_file_system/mock_native_file_system_permission_context.h"
#include "content/browser/native_file_system/native_file_system_directory_handle_impl.h"
#include "content/browser/native_file_system/native_file_system_file_handle_impl.h"
#include "content/browser/native_file_system/native_file_system_transfer_token_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace content {

using base::test::RunOnceCallback;
using blink::mojom::PermissionStatus;

class NativeFileSystemManagerImplTest : public testing::Test {
 public:
  NativeFileSystemManagerImplTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kNativeFileSystemAPI);
  }

  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    ASSERT_TRUE(dir_.GetPath().IsAbsolute());
    file_system_context_ = storage::CreateFileSystemContextForTesting(
        /*quota_manager_proxy=*/nullptr, dir_.GetPath());

    chrome_blob_context_ = base::MakeRefCounted<ChromeBlobStorageContext>();
    chrome_blob_context_->InitializeOnIOThread(base::FilePath(),
                                               base::FilePath(), nullptr);

    manager_ = base::MakeRefCounted<NativeFileSystemManagerImpl>(
        file_system_context_, chrome_blob_context_, &permission_context_,
        /*off_the_record=*/false);

    manager_->BindReceiver(kBindingContext,
                           manager_remote_.BindNewPipeAndPassReceiver());
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

  mojo::Remote<blink::mojom::NativeFileSystemDirectoryHandle>
  GetHandleForDirectory(const base::FilePath& path) {
    EXPECT_CALL(
        permission_context_,
        GetReadPermissionGrant(
            kTestOrigin, path, /*is_directory=*/true, kProcessId, kFrameId,
            NativeFileSystemPermissionContext::UserAction::kOpen))
        .WillOnce(testing::Return(allow_grant_));
    EXPECT_CALL(
        permission_context_,
        GetWritePermissionGrant(
            kTestOrigin, path, /*is_directory=*/true, kProcessId, kFrameId,
            NativeFileSystemPermissionContext::UserAction::kOpen))
        .WillOnce(testing::Return(allow_grant_));

    blink::mojom::NativeFileSystemEntryPtr entry =
        manager_->CreateDirectoryEntryFromPath(kBindingContext, path);
    return mojo::Remote<blink::mojom::NativeFileSystemDirectoryHandle>(
        std::move(entry->entry_handle->get_directory()));
  }

  NativeFileSystemTransferTokenImpl* SerializeAndDeserializeToken(
      mojo::PendingRemote<blink::mojom::NativeFileSystemTransferToken>
          token_remote) {
    std::vector<uint8_t> serialized;
    base::RunLoop serialize_loop;
    manager_->SerializeHandle(
        std::move(token_remote),
        base::BindLambdaForTesting([&](const std::vector<uint8_t>& bits) {
          EXPECT_FALSE(bits.empty());
          serialized = bits;
          serialize_loop.Quit();
        }));
    serialize_loop.Run();

    manager_->DeserializeHandle(kTestOrigin, serialized,
                                token_remote.InitWithNewPipeAndPassReceiver());
    base::RunLoop resolve_loop;
    NativeFileSystemTransferTokenImpl* result;
    manager_->ResolveTransferToken(
        std::move(token_remote),
        base::BindLambdaForTesting(
            [&](NativeFileSystemTransferTokenImpl* token) {
              result = token;
              resolve_loop.Quit();
            }));
    resolve_loop.Run();
    return result;
  }

 protected:
  const GURL kTestURL = GURL("https://example.com/test");
  const url::Origin kTestOrigin = url::Origin::Create(kTestURL);
  const int kProcessId = 1;
  const int kFrameId = 2;
  const NativeFileSystemManagerImpl::BindingContext kBindingContext = {
      kTestOrigin, kTestURL, kProcessId, kFrameId};

  base::test::ScopedFeatureList scoped_feature_list_;
  BrowserTaskEnvironment task_environment_;

  base::ScopedTempDir dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_refptr<ChromeBlobStorageContext> chrome_blob_context_;

  testing::StrictMock<MockNativeFileSystemPermissionContext>
      permission_context_;
  scoped_refptr<NativeFileSystemManagerImpl> manager_;
  mojo::Remote<blink::mojom::NativeFileSystemManager> manager_remote_;

  scoped_refptr<FixedNativeFileSystemPermissionGrant> ask_grant_ =
      base::MakeRefCounted<FixedNativeFileSystemPermissionGrant>(
          FixedNativeFileSystemPermissionGrant::PermissionStatus::ASK);
  scoped_refptr<FixedNativeFileSystemPermissionGrant> ask_grant2_ =
      base::MakeRefCounted<FixedNativeFileSystemPermissionGrant>(
          FixedNativeFileSystemPermissionGrant::PermissionStatus::ASK);
  scoped_refptr<FixedNativeFileSystemPermissionGrant> allow_grant_ =
      base::MakeRefCounted<FixedNativeFileSystemPermissionGrant>(
          FixedNativeFileSystemPermissionGrant::PermissionStatus::GRANTED);
};

TEST_F(NativeFileSystemManagerImplTest, GetSandboxedFileSystem_Permissions) {
  mojo::PendingRemote<blink::mojom::NativeFileSystemDirectoryHandle>
      directory_remote;
  base::RunLoop loop;
  manager_remote_->GetSandboxedFileSystem(base::BindLambdaForTesting(
      [&](blink::mojom::NativeFileSystemErrorPtr result,
          mojo::PendingRemote<blink::mojom::NativeFileSystemDirectoryHandle>
              handle) {
        EXPECT_EQ(blink::mojom::NativeFileSystemStatus::kOk, result->status);
        directory_remote = std::move(handle);
        loop.Quit();
      }));
  loop.Run();
  mojo::Remote<blink::mojom::NativeFileSystemDirectoryHandle> root(
      std::move(directory_remote));
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
      GetReadPermissionGrant(
          kTestOrigin, kTestPath, /*is_directory=*/false, kProcessId, kFrameId,
          NativeFileSystemPermissionContext::UserAction::kOpen))
      .WillOnce(testing::Return(allow_grant_));
  EXPECT_CALL(
      permission_context_,
      GetWritePermissionGrant(
          kTestOrigin, kTestPath, /*is_directory=*/false, kProcessId, kFrameId,
          NativeFileSystemPermissionContext::UserAction::kOpen))
      .WillOnce(testing::Return(ask_grant_));

  blink::mojom::NativeFileSystemEntryPtr entry =
      manager_->CreateFileEntryFromPath(kBindingContext, kTestPath);
  mojo::Remote<blink::mojom::NativeFileSystemFileHandle> handle(
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
      GetReadPermissionGrant(
          kTestOrigin, kTestPath, /*is_directory=*/false, kProcessId, kFrameId,
          NativeFileSystemPermissionContext::UserAction::kSave))
      .WillOnce(testing::Return(allow_grant_));
  EXPECT_CALL(
      permission_context_,
      GetWritePermissionGrant(
          kTestOrigin, kTestPath, /*is_directory=*/false, kProcessId, kFrameId,
          NativeFileSystemPermissionContext::UserAction::kSave))
      .WillOnce(testing::Return(allow_grant_));

  blink::mojom::NativeFileSystemEntryPtr entry =
      manager_->CreateWritableFileEntryFromPath(kBindingContext, kTestPath);
  mojo::Remote<blink::mojom::NativeFileSystemFileHandle> handle(
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
      GetReadPermissionGrant(
          kTestOrigin, kTestPath, /*is_directory=*/true, kProcessId, kFrameId,
          NativeFileSystemPermissionContext::UserAction::kOpen))
      .WillOnce(testing::Return(allow_grant_));
  EXPECT_CALL(
      permission_context_,
      GetWritePermissionGrant(
          kTestOrigin, kTestPath, /*is_directory=*/true, kProcessId, kFrameId,
          NativeFileSystemPermissionContext::UserAction::kOpen))
      .WillOnce(testing::Return(ask_grant_));

  blink::mojom::NativeFileSystemEntryPtr entry =
      manager_->CreateDirectoryEntryFromPath(kBindingContext, kTestPath);
  mojo::Remote<blink::mojom::NativeFileSystemDirectoryHandle> handle(
      std::move(entry->entry_handle->get_directory()));
  EXPECT_EQ(PermissionStatus::GRANTED,
            GetPermissionStatusSync(/*writable=*/false, handle.get()));
  EXPECT_EQ(PermissionStatus::ASK,
            GetPermissionStatusSync(/*writable=*/true, handle.get()));
}

TEST_F(NativeFileSystemManagerImplTest,
       FileWriterSwapDeletedOnConnectionClose) {
  auto test_file_url = file_system_context_->CreateCrackedFileSystemURL(
      kTestOrigin, storage::kFileSystemTypeTest,
      base::FilePath::FromUTF8Unsafe("test"));

  auto test_swap_url = file_system_context_->CreateCrackedFileSystemURL(
      kTestOrigin, storage::kFileSystemTypeTest,
      base::FilePath::FromUTF8Unsafe("test.crswap"));

  ASSERT_EQ(base::File::FILE_OK,
            storage::AsyncFileTestHelper::CreateFile(file_system_context_.get(),
                                                     test_file_url));

  ASSERT_EQ(base::File::FILE_OK,
            storage::AsyncFileTestHelper::CreateFile(file_system_context_.get(),
                                                     test_swap_url));

  mojo::Remote<blink::mojom::NativeFileSystemFileWriter> writer_remote(
      manager_->CreateFileWriter(kBindingContext, test_file_url, test_swap_url,
                                 NativeFileSystemManagerImpl::SharedHandleState(
                                     allow_grant_, allow_grant_, {})));

  ASSERT_TRUE(writer_remote.is_bound());
  ASSERT_TRUE(storage::AsyncFileTestHelper::FileExists(
      file_system_context_.get(), test_swap_url,
      storage::AsyncFileTestHelper::kDontCheckSize));

  // Severs the mojo pipe, causing the writer to be destroyed.
  writer_remote.reset();
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(storage::AsyncFileTestHelper::FileExists(
      file_system_context_.get(), test_swap_url,
      storage::AsyncFileTestHelper::kDontCheckSize));
}

TEST_F(NativeFileSystemManagerImplTest,
       FileWriterCloseAllowedToCompleteOnDestruct) {
  auto test_file_url = file_system_context_->CreateCrackedFileSystemURL(
      kTestOrigin, storage::kFileSystemTypeTest,
      base::FilePath::FromUTF8Unsafe("test"));

  auto test_swap_url = file_system_context_->CreateCrackedFileSystemURL(
      kTestOrigin, storage::kFileSystemTypeTest,
      base::FilePath::FromUTF8Unsafe("test.crswap"));

  ASSERT_EQ(base::File::FILE_OK,
            storage::AsyncFileTestHelper::CreateFileWithData(
                file_system_context_.get(), test_swap_url, "foo", 3));

  mojo::Remote<blink::mojom::NativeFileSystemFileWriter> writer_remote(
      manager_->CreateFileWriter(kBindingContext, test_file_url, test_swap_url,
                                 NativeFileSystemManagerImpl::SharedHandleState(
                                     allow_grant_, allow_grant_, {})));

  ASSERT_TRUE(writer_remote.is_bound());
  ASSERT_FALSE(storage::AsyncFileTestHelper::FileExists(
      file_system_context_.get(), test_file_url,
      storage::AsyncFileTestHelper::kDontCheckSize));
  writer_remote->Close(base::DoNothing());

  // Severs the mojo pipe, causing the writer to be destroyed.
  writer_remote.reset();
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(storage::AsyncFileTestHelper::FileExists(
      file_system_context_.get(), test_swap_url,
      storage::AsyncFileTestHelper::kDontCheckSize));
  ASSERT_TRUE(storage::AsyncFileTestHelper::FileExists(
      file_system_context_.get(), test_file_url, 3));
}

TEST_F(NativeFileSystemManagerImplTest, SerializeHandle_SandboxedFile) {
  auto test_file_url = file_system_context_->CreateCrackedFileSystemURL(
      kTestOrigin, storage::kFileSystemTypeTemporary,
      base::FilePath::FromUTF8Unsafe("test/foo/bar"));
  NativeFileSystemFileHandleImpl file(manager_.get(), kBindingContext,
                                      test_file_url,
                                      {ask_grant_, ask_grant_, {}});
  mojo::PendingRemote<blink::mojom::NativeFileSystemTransferToken> token_remote;
  manager_->CreateTransferToken(file,
                                token_remote.InitWithNewPipeAndPassReceiver());

  NativeFileSystemTransferTokenImpl* token =
      SerializeAndDeserializeToken(std::move(token_remote));
  ASSERT_TRUE(token);
  EXPECT_EQ(test_file_url, token->url());
  EXPECT_EQ(NativeFileSystemTransferTokenImpl::HandleType::kFile,
            token->type());

  // Deserialized sandboxed filesystem handles should always be readable and
  // writable.
  EXPECT_EQ(PermissionStatus::GRANTED,
            token->shared_handle_state().read_grant->GetStatus());
  EXPECT_EQ(PermissionStatus::GRANTED,
            token->shared_handle_state().write_grant->GetStatus());
}

TEST_F(NativeFileSystemManagerImplTest, SerializeHandle_SandboxedDirectory) {
  auto test_file_url = file_system_context_->CreateCrackedFileSystemURL(
      kTestOrigin, storage::kFileSystemTypeTemporary,
      base::FilePath::FromUTF8Unsafe("hello/world/"));
  NativeFileSystemDirectoryHandleImpl directory(manager_.get(), kBindingContext,
                                                test_file_url,
                                                {ask_grant_, ask_grant_, {}});
  mojo::PendingRemote<blink::mojom::NativeFileSystemTransferToken> token_remote;
  manager_->CreateTransferToken(directory,
                                token_remote.InitWithNewPipeAndPassReceiver());

  NativeFileSystemTransferTokenImpl* token =
      SerializeAndDeserializeToken(std::move(token_remote));
  ASSERT_TRUE(token);
  EXPECT_EQ(test_file_url, token->url());
  EXPECT_EQ(NativeFileSystemTransferTokenImpl::HandleType::kDirectory,
            token->type());

  // Deserialized sandboxed filesystem handles should always be readable and
  // writable.
  EXPECT_EQ(PermissionStatus::GRANTED,
            token->shared_handle_state().read_grant->GetStatus());
  EXPECT_EQ(PermissionStatus::GRANTED,
            token->shared_handle_state().write_grant->GetStatus());
}

TEST_F(NativeFileSystemManagerImplTest, SerializeHandle_Native_SingleFile) {
  const base::FilePath kTestPath(dir_.GetPath().AppendASCII("foo"));

  // Expect calls to get grants when creating the initial handle.
  EXPECT_CALL(
      permission_context_,
      GetReadPermissionGrant(
          kTestOrigin, kTestPath, /*is_directory=*/false, kProcessId, kFrameId,
          NativeFileSystemPermissionContext::UserAction::kOpen))
      .WillOnce(testing::Return(allow_grant_));
  EXPECT_CALL(
      permission_context_,
      GetWritePermissionGrant(
          kTestOrigin, kTestPath, /*is_directory=*/false, kProcessId, kFrameId,
          NativeFileSystemPermissionContext::UserAction::kOpen))
      .WillOnce(testing::Return(allow_grant_));

  blink::mojom::NativeFileSystemEntryPtr entry =
      manager_->CreateFileEntryFromPath(kBindingContext, kTestPath);
  mojo::Remote<blink::mojom::NativeFileSystemFileHandle> handle(
      std::move(entry->entry_handle->get_file()));

  mojo::PendingRemote<blink::mojom::NativeFileSystemTransferToken> token_remote;
  handle->Transfer(token_remote.InitWithNewPipeAndPassReceiver());

  // Deserializing tokens should re-request grants, with correct user action.
  EXPECT_CALL(
      permission_context_,
      GetReadPermissionGrant(
          kTestOrigin, kTestPath, /*is_directory=*/false,
          /*process_id=*/ChildProcessHost::kInvalidUniqueID,
          /*frame_id=*/MSG_ROUTING_NONE,
          NativeFileSystemPermissionContext::UserAction::kLoadFromStorage))
      .WillOnce(testing::Return(ask_grant_));
  EXPECT_CALL(
      permission_context_,
      GetWritePermissionGrant(
          kTestOrigin, kTestPath, /*is_directory=*/false,
          /*process_id=*/ChildProcessHost::kInvalidUniqueID,
          /*frame_id=*/MSG_ROUTING_NONE,
          NativeFileSystemPermissionContext::UserAction::kLoadFromStorage))
      .WillOnce(testing::Return(ask_grant2_));

  NativeFileSystemTransferTokenImpl* token =
      SerializeAndDeserializeToken(std::move(token_remote));
  ASSERT_TRUE(token);
  EXPECT_EQ(kTestOrigin, token->url().origin());
  EXPECT_EQ(kTestPath, token->url().path());
  EXPECT_EQ(storage::kFileSystemTypeNativeLocal, token->url().type());
  EXPECT_EQ(storage::kFileSystemTypeIsolated, token->url().mount_type());
  EXPECT_EQ(NativeFileSystemTransferTokenImpl::HandleType::kFile,
            token->type());
  EXPECT_EQ(ask_grant_, token->shared_handle_state().read_grant);
  EXPECT_EQ(ask_grant2_, token->shared_handle_state().write_grant);
}

TEST_F(NativeFileSystemManagerImplTest,
       SerializeHandle_Native_SingleDirectory) {
  const base::FilePath kTestPath(dir_.GetPath().AppendASCII("foobar"));
  mojo::Remote<blink::mojom::NativeFileSystemDirectoryHandle> handle =
      GetHandleForDirectory(kTestPath);

  mojo::PendingRemote<blink::mojom::NativeFileSystemTransferToken> token_remote;
  handle->Transfer(token_remote.InitWithNewPipeAndPassReceiver());

  // Deserializing tokens should re-request grants, with correct user action.
  EXPECT_CALL(
      permission_context_,
      GetReadPermissionGrant(
          kTestOrigin, kTestPath, /*is_directory=*/true,
          /*process_id=*/ChildProcessHost::kInvalidUniqueID,
          /*frame_id=*/MSG_ROUTING_NONE,
          NativeFileSystemPermissionContext::UserAction::kLoadFromStorage))
      .WillOnce(testing::Return(ask_grant_));
  EXPECT_CALL(
      permission_context_,
      GetWritePermissionGrant(
          kTestOrigin, kTestPath, /*is_directory=*/true,
          /*process_id=*/ChildProcessHost::kInvalidUniqueID,
          /*frame_id=*/MSG_ROUTING_NONE,
          NativeFileSystemPermissionContext::UserAction::kLoadFromStorage))
      .WillOnce(testing::Return(ask_grant2_));

  NativeFileSystemTransferTokenImpl* token =
      SerializeAndDeserializeToken(std::move(token_remote));
  ASSERT_TRUE(token);
  EXPECT_EQ(kTestOrigin, token->url().origin());
  EXPECT_EQ(kTestPath, token->url().path());
  EXPECT_EQ(storage::kFileSystemTypeNativeLocal, token->url().type());
  EXPECT_EQ(storage::kFileSystemTypeIsolated, token->url().mount_type());
  EXPECT_EQ(NativeFileSystemTransferTokenImpl::HandleType::kDirectory,
            token->type());
  EXPECT_EQ(ask_grant_, token->shared_handle_state().read_grant);
  EXPECT_EQ(ask_grant2_, token->shared_handle_state().write_grant);
}

TEST_F(NativeFileSystemManagerImplTest,
       SerializeHandle_Native_FileInsideDirectory) {
  const base::FilePath kDirectoryPath(dir_.GetPath().AppendASCII("foo"));
  const std::string kTestName = "test file name ☺";
  base::CreateDirectory(kDirectoryPath);

  mojo::Remote<blink::mojom::NativeFileSystemDirectoryHandle> directory_handle =
      GetHandleForDirectory(kDirectoryPath);

  mojo::Remote<blink::mojom::NativeFileSystemFileHandle> file_handle;
  base::RunLoop get_file_loop;
  directory_handle->GetFile(
      kTestName, /*create=*/true,
      base::BindLambdaForTesting(
          [&](blink::mojom::NativeFileSystemErrorPtr result,
              mojo::PendingRemote<blink::mojom::NativeFileSystemFileHandle>
                  handle) {
            get_file_loop.Quit();
            ASSERT_EQ(blink::mojom::NativeFileSystemStatus::kOk,
                      result->status);
            file_handle.Bind(std::move(handle));
          }));
  get_file_loop.Run();
  ASSERT_TRUE(file_handle.is_bound());

  mojo::PendingRemote<blink::mojom::NativeFileSystemTransferToken> token_remote;
  file_handle->Transfer(token_remote.InitWithNewPipeAndPassReceiver());

  // Deserializing tokens should re-request grants, with correct user action.
  EXPECT_CALL(
      permission_context_,
      GetReadPermissionGrant(
          kTestOrigin, kDirectoryPath, /*is_directory=*/true,
          /*process_id=*/ChildProcessHost::kInvalidUniqueID,
          /*frame_id=*/MSG_ROUTING_NONE,
          NativeFileSystemPermissionContext::UserAction::kLoadFromStorage))
      .WillOnce(testing::Return(ask_grant_));
  EXPECT_CALL(
      permission_context_,
      GetWritePermissionGrant(
          kTestOrigin, kDirectoryPath, /*is_directory=*/true,
          /*process_id=*/ChildProcessHost::kInvalidUniqueID,
          /*frame_id=*/MSG_ROUTING_NONE,
          NativeFileSystemPermissionContext::UserAction::kLoadFromStorage))
      .WillOnce(testing::Return(ask_grant2_));

  NativeFileSystemTransferTokenImpl* token =
      SerializeAndDeserializeToken(std::move(token_remote));
  ASSERT_TRUE(token);
  EXPECT_EQ(kTestOrigin, token->url().origin());
  EXPECT_EQ(kDirectoryPath.Append(base::FilePath::FromUTF8Unsafe(kTestName)),
            token->url().path());
  EXPECT_EQ(storage::kFileSystemTypeNativeLocal, token->url().type());
  EXPECT_EQ(storage::kFileSystemTypeIsolated, token->url().mount_type());
  EXPECT_EQ(NativeFileSystemTransferTokenImpl::HandleType::kFile,
            token->type());
  EXPECT_EQ(ask_grant_, token->shared_handle_state().read_grant);
  EXPECT_EQ(ask_grant2_, token->shared_handle_state().write_grant);
}

TEST_F(NativeFileSystemManagerImplTest,
       SerializeHandle_Native_DirectoryInsideDirectory) {
  const base::FilePath kDirectoryPath(dir_.GetPath().AppendASCII("foo"));
  const std::string kTestName = "test dir name";
  base::CreateDirectory(kDirectoryPath);

  mojo::Remote<blink::mojom::NativeFileSystemDirectoryHandle> directory_handle =
      GetHandleForDirectory(kDirectoryPath);

  mojo::Remote<blink::mojom::NativeFileSystemDirectoryHandle> child_handle;
  base::RunLoop get_directory_loop;
  directory_handle->GetDirectory(
      kTestName, /*create=*/true,
      base::BindLambdaForTesting(
          [&](blink::mojom::NativeFileSystemErrorPtr result,
              mojo::PendingRemote<blink::mojom::NativeFileSystemDirectoryHandle>
                  handle) {
            get_directory_loop.Quit();
            ASSERT_EQ(blink::mojom::NativeFileSystemStatus::kOk,
                      result->status);
            child_handle.Bind(std::move(handle));
          }));
  get_directory_loop.Run();
  ASSERT_TRUE(child_handle.is_bound());

  mojo::PendingRemote<blink::mojom::NativeFileSystemTransferToken> token_remote;
  child_handle->Transfer(token_remote.InitWithNewPipeAndPassReceiver());

  // Deserializing tokens should re-request grants, with correct user action.
  EXPECT_CALL(
      permission_context_,
      GetReadPermissionGrant(
          kTestOrigin, kDirectoryPath, /*is_directory=*/true,
          /*process_id=*/ChildProcessHost::kInvalidUniqueID,
          /*frame_id=*/MSG_ROUTING_NONE,
          NativeFileSystemPermissionContext::UserAction::kLoadFromStorage))
      .WillOnce(testing::Return(ask_grant_));
  EXPECT_CALL(
      permission_context_,
      GetWritePermissionGrant(
          kTestOrigin, kDirectoryPath, /*is_directory=*/true,
          /*process_id=*/ChildProcessHost::kInvalidUniqueID,
          /*frame_id=*/MSG_ROUTING_NONE,
          NativeFileSystemPermissionContext::UserAction::kLoadFromStorage))
      .WillOnce(testing::Return(ask_grant2_));

  NativeFileSystemTransferTokenImpl* token =
      SerializeAndDeserializeToken(std::move(token_remote));
  ASSERT_TRUE(token);
  EXPECT_EQ(kTestOrigin, token->url().origin());
  EXPECT_EQ(kDirectoryPath.AppendASCII(kTestName), token->url().path());
  EXPECT_EQ(storage::kFileSystemTypeNativeLocal, token->url().type());
  EXPECT_EQ(storage::kFileSystemTypeIsolated, token->url().mount_type());
  EXPECT_EQ(NativeFileSystemTransferTokenImpl::HandleType::kDirectory,
            token->type());
  EXPECT_EQ(ask_grant_, token->shared_handle_state().read_grant);
  EXPECT_EQ(ask_grant2_, token->shared_handle_state().write_grant);
}

}  // namespace content
