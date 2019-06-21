// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_file_system/native_file_system_manager_impl.h"

#include "base/files/file_path.h"
#include "base/task/post_task.h"
#include "content/browser/native_file_system/file_system_chooser.h"
#include "content/browser/native_file_system/fixed_native_file_system_permission_grant.h"
#include "content/browser/native_file_system/native_file_system_directory_handle_impl.h"
#include "content/browser/native_file_system/native_file_system_file_handle_impl.h"
#include "content/browser/native_file_system/native_file_system_transfer_token_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_operation_runner.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/browser/fileapi/isolated_context.h"
#include "storage/common/fileapi/file_system_util.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_error.mojom.h"
#include "url/origin.h"

namespace content {

using blink::mojom::NativeFileSystemError;
using PermissionStatus = NativeFileSystemPermissionGrant::PermissionStatus;

NativeFileSystemManagerImpl::SharedHandleState::SharedHandleState(
    scoped_refptr<NativeFileSystemPermissionGrant> read_grant,
    scoped_refptr<NativeFileSystemPermissionGrant> write_grant,
    storage::IsolatedContext::ScopedFSHandle file_system)
    : read_grant(std::move(read_grant)),
      write_grant(std::move(write_grant)),
      file_system(std::move(file_system)) {
  DCHECK(this->read_grant);
  DCHECK(this->write_grant);
}

NativeFileSystemManagerImpl::SharedHandleState::SharedHandleState(
    const SharedHandleState& other) = default;
NativeFileSystemManagerImpl::SharedHandleState::~SharedHandleState() = default;

NativeFileSystemManagerImpl::NativeFileSystemManagerImpl(
    scoped_refptr<storage::FileSystemContext> context,
    scoped_refptr<ChromeBlobStorageContext> blob_context,
    NativeFileSystemPermissionContext* permission_context)
    : context_(std::move(context)),
      blob_context_(std::move(blob_context)),
      permission_context_(permission_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(context_);
  DCHECK(blob_context_);
}

NativeFileSystemManagerImpl::~NativeFileSystemManagerImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void NativeFileSystemManagerImpl::BindRequest(
    const BindingContext& binding_context,
    blink::mojom::NativeFileSystemManagerRequest request) {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kNativeFileSystemAPI));

  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  bindings_.AddBinding(this, std::move(request), binding_context);
}

void NativeFileSystemManagerImpl::GetSandboxedFileSystem(
    GetSandboxedFileSystemCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  url::Origin origin = bindings_.dispatch_context().origin;

  context()->OpenFileSystem(
      origin.GetURL(), storage::kFileSystemTypeTemporary,
      storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
      base::BindOnce(&NativeFileSystemManagerImpl::DidOpenSandboxedFileSystem,
                     weak_factory_.GetWeakPtr(), bindings_.dispatch_context(),
                     std::move(callback)));
}

void NativeFileSystemManagerImpl::ChooseEntries(
    blink::mojom::ChooseFileSystemEntryType type,
    std::vector<blink::mojom::ChooseFileSystemEntryAcceptsOptionPtr> accepts,
    bool include_accepts_all,
    ChooseEntriesCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  const BindingContext& context = bindings_.dispatch_context();

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &FileSystemChooser::CreateAndShow, context.process_id,
          context.frame_id, type, std::move(accepts), include_accepts_all,
          base::BindOnce(&NativeFileSystemManagerImpl::DidChooseEntries,
                         weak_factory_.GetWeakPtr(), context, type,
                         std::move(callback)),
          base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO})));
}

blink::mojom::NativeFileSystemFileHandlePtr
NativeFileSystemManagerImpl::CreateFileHandle(
    const BindingContext& binding_context,
    const storage::FileSystemURL& url,
    const SharedHandleState& handle_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(url.is_valid());
  DCHECK_EQ(url.mount_type() == storage::kFileSystemTypeIsolated,
            handle_state.file_system.is_valid())
      << url.mount_type();

  blink::mojom::NativeFileSystemFileHandlePtr result;
  file_bindings_.AddBinding(std::make_unique<NativeFileSystemFileHandleImpl>(
                                this, binding_context, url, handle_state),
                            mojo::MakeRequest(&result));
  return result;
}

blink::mojom::NativeFileSystemDirectoryHandlePtr
NativeFileSystemManagerImpl::CreateDirectoryHandle(
    const BindingContext& binding_context,
    const storage::FileSystemURL& url,
    const SharedHandleState& handle_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(url.is_valid());
  DCHECK_EQ(url.mount_type() == storage::kFileSystemTypeIsolated,
            handle_state.file_system.is_valid())
      << url.mount_type();

  blink::mojom::NativeFileSystemDirectoryHandlePtr result;
  directory_bindings_.AddBinding(
      std::make_unique<NativeFileSystemDirectoryHandleImpl>(
          this, binding_context, url, handle_state),
      mojo::MakeRequest(&result));
  return result;
}

blink::mojom::NativeFileSystemEntryPtr
NativeFileSystemManagerImpl::CreateFileEntryFromPath(
    const BindingContext& binding_context,
    const base::FilePath& file_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto url = CreateFileSystemURLFromPath(binding_context.origin, file_path);

  auto read_grant = base::MakeRefCounted<FixedNativeFileSystemPermissionGrant>(
      PermissionStatus::GRANTED);
  scoped_refptr<NativeFileSystemPermissionGrant> write_grant;
  if (permission_context_) {
    write_grant = permission_context_->GetWritePermissionGrant(
        binding_context.origin, file_path, /*is_directory=*/false);
  } else {
    // Auto-deny all write grants if no permisson context is available, unless
    // Experimental Web Platform features are enabled.
    // TODO(mek): Remove experimental web platform check when permission UI is
    // implemented.
    write_grant = base::MakeRefCounted<FixedNativeFileSystemPermissionGrant>(
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableExperimentalWebPlatformFeatures)
            ? PermissionStatus::GRANTED
            : PermissionStatus::DENIED);
  }

  return blink::mojom::NativeFileSystemEntry::New(
      blink::mojom::NativeFileSystemHandle::NewFile(
          CreateFileHandle(
              binding_context, url.url,
              SharedHandleState(std::move(read_grant), std::move(write_grant),
                                std::move(url.file_system)))
              .PassInterface()),
      url.base_name);
}

blink::mojom::NativeFileSystemEntryPtr
NativeFileSystemManagerImpl::CreateDirectoryEntryFromPath(
    const BindingContext& binding_context,
    const base::FilePath& directory_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto url =
      CreateFileSystemURLFromPath(binding_context.origin, directory_path);

  auto read_grant = base::MakeRefCounted<FixedNativeFileSystemPermissionGrant>(
      PermissionStatus::GRANTED);
  scoped_refptr<NativeFileSystemPermissionGrant> write_grant;
  if (permission_context_) {
    write_grant = permission_context_->GetWritePermissionGrant(
        binding_context.origin, directory_path, /*is_directory=*/true);
  } else {
    // Auto-deny all write grants if no permisson context is available, unless
    // Experimental Web Platform features are enabled.
    // TODO(mek): Remove experimental web platform check when permission UI is
    // implemented.
    write_grant = base::MakeRefCounted<FixedNativeFileSystemPermissionGrant>(
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableExperimentalWebPlatformFeatures)
            ? PermissionStatus::GRANTED
            : PermissionStatus::DENIED);
  }

  return blink::mojom::NativeFileSystemEntry::New(
      blink::mojom::NativeFileSystemHandle::NewDirectory(
          CreateDirectoryHandle(
              binding_context, url.url,
              SharedHandleState(std::move(read_grant), std::move(write_grant),
                                std::move(url.file_system)))
              .PassInterface()),
      url.base_name);
}

void NativeFileSystemManagerImpl::CreateTransferToken(
    const NativeFileSystemFileHandleImpl& file,
    blink::mojom::NativeFileSystemTransferTokenRequest request) {
  return CreateTransferTokenImpl(file.url(), file.handle_state(),
                                 /*is_directory=*/false, std::move(request));
}

void NativeFileSystemManagerImpl::CreateTransferToken(
    const NativeFileSystemDirectoryHandleImpl& directory,
    blink::mojom::NativeFileSystemTransferTokenRequest request) {
  return CreateTransferTokenImpl(directory.url(), directory.handle_state(),
                                 /*is_directory=*/true, std::move(request));
}

void NativeFileSystemManagerImpl::ResolveTransferToken(
    blink::mojom::NativeFileSystemTransferTokenPtr token,
    ResolvedTokenCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto* raw_token = token.get();
  raw_token->GetInternalID(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      base::BindOnce(&NativeFileSystemManagerImpl::DoResolveTransferToken,
                     weak_factory_.GetWeakPtr(), std::move(token),
                     std::move(callback)),
      base::UnguessableToken()));
}

storage::FileSystemOperationRunner*
NativeFileSystemManagerImpl::operation_runner() {
  if (!operation_runner_)
    operation_runner_ = context()->CreateFileSystemOperationRunner();
  return operation_runner_.get();
}

void NativeFileSystemManagerImpl::DidOpenSandboxedFileSystem(
    const BindingContext& binding_context,
    GetSandboxedFileSystemCallback callback,
    const GURL& root,
    const std::string& filesystem_name,
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (result != base::File::FILE_OK) {
    std::move(callback).Run(NativeFileSystemError::New(result), nullptr);
    return;
  }

  auto permission_grant =
      base::MakeRefCounted<FixedNativeFileSystemPermissionGrant>(
          PermissionStatus::GRANTED);

  std::move(callback).Run(
      NativeFileSystemError::New(base::File::FILE_OK),
      CreateDirectoryHandle(
          binding_context, context()->CrackURL(root),
          SharedHandleState(permission_grant, permission_grant,
                            /*file_system=*/{})));
}

void NativeFileSystemManagerImpl::DidChooseEntries(
    const BindingContext& binding_context,
    blink::mojom::ChooseFileSystemEntryType type,
    ChooseEntriesCallback callback,
    blink::mojom::NativeFileSystemErrorPtr result,
    std::vector<base::FilePath> entries) {
  std::vector<blink::mojom::NativeFileSystemEntryPtr> result_entries;
  if (result->error_code != base::File::FILE_OK) {
    std::move(callback).Run(std::move(result), std::move(result_entries));
    return;
  }
  result_entries.reserve(entries.size());
  for (const auto& entry : entries) {
    if (type == blink::mojom::ChooseFileSystemEntryType::kOpenDirectory) {
      result_entries.push_back(
          CreateDirectoryEntryFromPath(binding_context, entry));
    } else {
      result_entries.push_back(CreateFileEntryFromPath(binding_context, entry));
    }
  }
  // TODO(mek): Auto-grant write permission if this was a file as a result of a
  // save dialog.
  std::move(callback).Run(std::move(result), std::move(result_entries));
}

void NativeFileSystemManagerImpl::CreateTransferTokenImpl(
    const storage::FileSystemURL& url,
    const SharedHandleState& handle_state,
    bool is_directory,
    blink::mojom::NativeFileSystemTransferTokenRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto token_impl = std::make_unique<NativeFileSystemTransferTokenImpl>(
      url, handle_state,
      is_directory ? NativeFileSystemTransferTokenImpl::HandleType::kDirectory
                   : NativeFileSystemTransferTokenImpl::HandleType::kFile);
  auto token = token_impl->token();
  blink::mojom::NativeFileSystemTransferTokenPtr result;
  auto emplace_result = transfer_tokens_.emplace(
      std::piecewise_construct, std::forward_as_tuple(token),
      std::forward_as_tuple(std::move(token_impl), std::move(request)));
  DCHECK(emplace_result.second);
  emplace_result.first->second.set_connection_error_handler(base::BindOnce(
      &NativeFileSystemManagerImpl::TransferTokenConnectionErrorHandler,
      base::Unretained(this), token));
}

void NativeFileSystemManagerImpl::TransferTokenConnectionErrorHandler(
    const base::UnguessableToken& token) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  size_t count_removed = transfer_tokens_.erase(token);
  DCHECK_EQ(1u, count_removed);
}

void NativeFileSystemManagerImpl::DoResolveTransferToken(
    blink::mojom::NativeFileSystemTransferTokenPtr,
    ResolvedTokenCallback callback,
    const base::UnguessableToken& token) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto it = transfer_tokens_.find(token);
  if (it == transfer_tokens_.end()) {
    std::move(callback).Run(nullptr);
  } else {
    std::move(callback).Run(
        static_cast<NativeFileSystemTransferTokenImpl*>(it->second.impl()));
  }
}

NativeFileSystemManagerImpl::FileSystemURLAndFSHandle
NativeFileSystemManagerImpl::CreateFileSystemURLFromPath(
    const url::Origin& origin,
    const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto* isolated_context = storage::IsolatedContext::GetInstance();
  DCHECK(isolated_context);

  FileSystemURLAndFSHandle result;

  result.file_system = isolated_context->RegisterFileSystemForPath(
      storage::kFileSystemTypeNativeLocal, std::string(), path,
      &result.base_name);

  base::FilePath root_path =
      isolated_context->CreateVirtualRootPath(result.file_system.id());
  base::FilePath isolated_path = root_path.AppendASCII(result.base_name);

  result.url = context()->CreateCrackedFileSystemURL(
      origin.GetURL(), storage::kFileSystemTypeIsolated, isolated_path);
  return result;
}

}  // namespace content