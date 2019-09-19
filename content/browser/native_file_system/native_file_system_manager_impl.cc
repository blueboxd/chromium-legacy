// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_file_system/native_file_system_manager_impl.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/post_task.h"
#include "content/browser/native_file_system/file_system_chooser.h"
#include "content/browser/native_file_system/fixed_native_file_system_permission_grant.h"
#include "content/browser/native_file_system/native_file_system_directory_handle_impl.h"
#include "content/browser/native_file_system/native_file_system_error.h"
#include "content/browser/native_file_system/native_file_system_file_handle_impl.h"
#include "content/browser/native_file_system/native_file_system_file_writer_impl.h"
#include "content/browser/native_file_system/native_file_system_transfer_token_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_operation_runner.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/browser/fileapi/isolated_context.h"
#include "storage/common/fileapi/file_system_util.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_error.mojom.h"
#include "url/origin.h"

namespace content {

using blink::mojom::NativeFileSystemStatus;
using PermissionStatus = NativeFileSystemPermissionGrant::PermissionStatus;
using SensitiveDirectoryResult =
    NativeFileSystemPermissionContext::SensitiveDirectoryResult;

namespace {

void ShowFilePickerOnUIThread(const url::Origin& requesting_origin,
                              int render_process_id,
                              int frame_id,
                              bool require_user_gesture,
                              const FileSystemChooser::Options& options,
                              FileSystemChooser::ResultCallback callback,
                              scoped_refptr<base::TaskRunner> callback_runner) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHost* rfh = RenderFrameHost::FromID(render_process_id, frame_id);
  WebContents* web_contents = WebContents::FromRenderFrameHost(rfh);

  if (!web_contents) {
    callback_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       native_file_system_error::FromStatus(
                           NativeFileSystemStatus::kOperationAborted),
                       std::vector<base::FilePath>()));
    return;
  }

  url::Origin embedding_origin =
      url::Origin::Create(web_contents->GetLastCommittedURL());
  if (embedding_origin != requesting_origin) {
    // Third party iframes are not allowed to show a file picker.
    callback_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            native_file_system_error::FromStatus(
                NativeFileSystemStatus::kPermissionDenied,
                "Third party iframes are not allowed to show a file picker."),
            std::vector<base::FilePath>()));
    return;
  }

  // Renderer process should already check for user activation before sending
  // IPC, but just to be sure double check here as well. This is not treated
  // as a BadMessage because it is possible for the transient user activation
  // to expire between the renderer side check and this check.
  if (require_user_gesture && !rfh->HasTransientUserActivation()) {
    callback_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            native_file_system_error::FromStatus(
                NativeFileSystemStatus::kPermissionDenied,
                "User activation is required to show a file picker."),
            std::vector<base::FilePath>()));
    return;
  }

  FileSystemChooser::CreateAndShow(web_contents, options, std::move(callback),
                                   std::move(callback_runner));
}

bool HasTransientUserActivation(int render_process_id, int frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHost* rfh = RenderFrameHost::FromID(render_process_id, frame_id);

  if (!rfh)
    return false;

  return rfh->HasTransientUserActivation();
}

bool CreateOrTruncateFile(const base::FilePath& path) {
  int creation_flags = base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE;
  base::File file(path, creation_flags);
  return file.IsValid();
}

}  // namespace

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
    NativeFileSystemPermissionContext* permission_context,
    bool off_the_record)
    : context_(std::move(context)),
      blob_context_(std::move(blob_context)),
      permission_context_(permission_context),
      off_the_record_(off_the_record) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(context_);
  DCHECK(blob_context_);
}

NativeFileSystemManagerImpl::~NativeFileSystemManagerImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void NativeFileSystemManagerImpl::BindReceiver(
    const BindingContext& binding_context,
    mojo::PendingReceiver<blink::mojom::NativeFileSystemManager> receiver) {
  DCHECK(base::FeatureList::IsEnabled(blink::features::kNativeFileSystemAPI));

  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DCHECK(network::IsOriginPotentiallyTrustworthy(binding_context.origin));
  receivers_.Add(this, std::move(receiver), binding_context);
}

// static
void NativeFileSystemManagerImpl::BindReceiverFromUIThread(
    StoragePartitionImpl* storage_partition,
    const BindingContext& binding_context,
    mojo::PendingReceiver<blink::mojom::NativeFileSystemManager> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!network::IsOriginPotentiallyTrustworthy(binding_context.origin)) {
    mojo::ReportBadMessage("Native File System access from Unsecure Origin");
    return;
  }

  auto* manager = storage_partition->GetNativeFileSystemManager();
  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(&NativeFileSystemManagerImpl::BindReceiver,
                                base::Unretained(manager), binding_context,
                                std::move(receiver)));
}

void NativeFileSystemManagerImpl::GetSandboxedFileSystem(
    GetSandboxedFileSystemCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  url::Origin origin = receivers_.current_context().origin;

  context()->OpenFileSystem(
      origin.GetURL(), storage::kFileSystemTypeTemporary,
      storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
      base::BindOnce(&NativeFileSystemManagerImpl::DidOpenSandboxedFileSystem,
                     weak_factory_.GetWeakPtr(), receivers_.current_context(),
                     std::move(callback)));
}

void NativeFileSystemManagerImpl::ChooseEntries(
    blink::mojom::ChooseFileSystemEntryType type,
    std::vector<blink::mojom::ChooseFileSystemEntryAcceptsOptionPtr> accepts,
    bool include_accepts_all,
    ChooseEntriesCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  const BindingContext& context = receivers_.current_context();

  // ChooseEntries API is only available to windows, as we need a frame to
  // anchor the picker to.
  if (context.is_worker()) {
    receivers_.ReportBadMessage("ChooseEntries called from a worker");
    return;
  }

  // When site setting is block, it's better not to show file chooser for save.
  if (type == blink::mojom::ChooseFileSystemEntryType::kSaveFile &&
      permission_context_ &&
      !permission_context_->CanRequestWritePermission(context.origin)) {
    std::move(callback).Run(
        native_file_system_error::FromStatus(
            NativeFileSystemStatus::kPermissionDenied),
        std::vector<blink::mojom::NativeFileSystemEntryPtr>());

    return;
  }

  FileSystemChooser::Options options(type, std::move(accepts),
                                     include_accepts_all);
  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &ShowFilePickerOnUIThread, context.origin, context.process_id,
          context.frame_id, /*require_user_gesture=*/true, options,
          base::BindOnce(&NativeFileSystemManagerImpl::DidChooseEntries,
                         weak_factory_.GetWeakPtr(), context, options,
                         std::move(callback)),
          base::CreateSingleThreadTaskRunner({BrowserThread::IO})));
}

blink::mojom::NativeFileSystemEntryPtr
NativeFileSystemManagerImpl::CreateFileEntryFromPath(
    const BindingContext& binding_context,
    const base::FilePath& file_path) {
  return CreateFileEntryFromPathImpl(
      binding_context, file_path,
      NativeFileSystemPermissionContext::UserAction::kOpen);
}

blink::mojom::NativeFileSystemEntryPtr
NativeFileSystemManagerImpl::CreateDirectoryEntryFromPath(
    const BindingContext& binding_context,
    const base::FilePath& directory_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto url =
      CreateFileSystemURLFromPath(binding_context.origin, directory_path);

  scoped_refptr<NativeFileSystemPermissionGrant> read_grant, write_grant;
  if (permission_context_) {
    read_grant = permission_context_->GetReadPermissionGrant(
        binding_context.origin, directory_path, /*is_directory=*/true,
        binding_context.process_id, binding_context.frame_id);
    write_grant = permission_context_->GetWritePermissionGrant(
        binding_context.origin, directory_path, /*is_directory=*/true,
        binding_context.process_id, binding_context.frame_id,
        NativeFileSystemPermissionContext::UserAction::kOpen);
  } else {
    // Grant read permission even without a permission_context_, as the picker
    // itself is enough UI to assume user intent.
    read_grant = base::MakeRefCounted<FixedNativeFileSystemPermissionGrant>(
        PermissionStatus::GRANTED);
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

blink::mojom::NativeFileSystemEntryPtr
NativeFileSystemManagerImpl::CreateWritableFileEntryFromPath(
    const BindingContext& binding_context,
    const base::FilePath& file_path) {
  return CreateFileEntryFromPathImpl(
      binding_context, file_path,
      NativeFileSystemPermissionContext::UserAction::kSave);
}

mojo::PendingRemote<blink::mojom::NativeFileSystemFileHandle>
NativeFileSystemManagerImpl::CreateFileHandle(
    const BindingContext& binding_context,
    const storage::FileSystemURL& url,
    const SharedHandleState& handle_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(url.is_valid());
  DCHECK_EQ(url.mount_type() == storage::kFileSystemTypeIsolated,
            handle_state.file_system.is_valid())
      << url.mount_type();

  mojo::PendingRemote<blink::mojom::NativeFileSystemFileHandle> result;
  file_receivers_.Add(std::make_unique<NativeFileSystemFileHandleImpl>(
                          this, binding_context, url, handle_state),
                      result.InitWithNewPipeAndPassReceiver());
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

mojo::PendingRemote<blink::mojom::NativeFileSystemFileWriter>
NativeFileSystemManagerImpl::CreateFileWriter(
    const BindingContext& binding_context,
    const storage::FileSystemURL& url,
    const storage::FileSystemURL& swap_url,
    const SharedHandleState& handle_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  mojo::PendingRemote<blink::mojom::NativeFileSystemFileWriter> result;
  mojo::PendingReceiver<blink::mojom::NativeFileSystemFileWriter>
      writer_receiver = result.InitWithNewPipeAndPassReceiver();

  base::PostTaskAndReplyWithResult(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&HasTransientUserActivation, binding_context.process_id,
                     binding_context.frame_id),
      base::BindOnce(&NativeFileSystemManagerImpl::CreateFileWriterImpl,
                     weak_factory_.GetWeakPtr(), binding_context, url, swap_url,
                     handle_state, std::move(writer_receiver)));
  return result;
}

void NativeFileSystemManagerImpl::CreateTransferToken(
    const NativeFileSystemFileHandleImpl& file,
    mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken>
        receiver) {
  return CreateTransferTokenImpl(file.url(), file.handle_state(),
                                 /*is_directory=*/false, std::move(receiver));
}

void NativeFileSystemManagerImpl::CreateTransferToken(
    const NativeFileSystemDirectoryHandleImpl& directory,
    mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken>
        receiver) {
  return CreateTransferTokenImpl(directory.url(), directory.handle_state(),
                                 /*is_directory=*/true, std::move(receiver));
}

void NativeFileSystemManagerImpl::ResolveTransferToken(
    mojo::PendingRemote<blink::mojom::NativeFileSystemTransferToken> token,
    ResolvedTokenCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  mojo::Remote<blink::mojom::NativeFileSystemTransferToken> token_remote(
      std::move(token));
  auto* raw_token = token_remote.get();
  raw_token->GetInternalID(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      base::BindOnce(&NativeFileSystemManagerImpl::DoResolveTransferToken,
                     weak_factory_.GetWeakPtr(), std::move(token_remote),
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
    std::move(callback).Run(native_file_system_error::FromFileError(result),
                            nullptr);
    return;
  }

  auto permission_grant =
      base::MakeRefCounted<FixedNativeFileSystemPermissionGrant>(
          PermissionStatus::GRANTED);

  std::move(callback).Run(
      native_file_system_error::Ok(),
      CreateDirectoryHandle(
          binding_context, context()->CrackURL(root),
          SharedHandleState(permission_grant, permission_grant,
                            /*file_system=*/{})));
}

void NativeFileSystemManagerImpl::DidChooseEntries(
    const BindingContext& binding_context,
    const FileSystemChooser::Options& options,
    ChooseEntriesCallback callback,
    blink::mojom::NativeFileSystemErrorPtr result,
    std::vector<base::FilePath> entries) {
  if (result->status != NativeFileSystemStatus::kOk) {
    std::move(callback).Run(
        std::move(result),
        std::vector<blink::mojom::NativeFileSystemEntryPtr>());
    return;
  }

  if (!permission_context_) {
    DidVerifySensitiveDirectoryAccess(binding_context, options,
                                      std::move(callback), std::move(entries),
                                      SensitiveDirectoryResult::kAllowed);
    return;
  }
  auto entries_copy = entries;
  const bool is_directory =
      options.type() == blink::mojom::ChooseFileSystemEntryType::kOpenDirectory;
  permission_context_->ConfirmSensitiveDirectoryAccess(
      binding_context.origin, entries_copy, is_directory,
      binding_context.process_id, binding_context.frame_id,
      base::BindOnce(
          &NativeFileSystemManagerImpl::DidVerifySensitiveDirectoryAccess,
          weak_factory_.GetWeakPtr(), binding_context, options,
          std::move(callback), std::move(entries)));
}

void NativeFileSystemManagerImpl::DidVerifySensitiveDirectoryAccess(
    const BindingContext& binding_context,
    const FileSystemChooser::Options& options,
    ChooseEntriesCallback callback,
    std::vector<base::FilePath> entries,
    SensitiveDirectoryResult result) {
  base::UmaHistogramEnumeration(
      "NativeFileSystemAPI.SensitiveDirectoryAccessResult", result);

  if (result == SensitiveDirectoryResult::kAbort) {
    std::move(callback).Run(
        native_file_system_error::FromStatus(
            NativeFileSystemStatus::kOperationAborted),
        std::vector<blink::mojom::NativeFileSystemEntryPtr>());
    return;
  }
  if (result == SensitiveDirectoryResult::kTryAgain) {
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(
            &ShowFilePickerOnUIThread, binding_context.origin,
            binding_context.process_id, binding_context.frame_id,
            /*require_user_gesture=*/false, options,
            base::BindOnce(&NativeFileSystemManagerImpl::DidChooseEntries,
                           weak_factory_.GetWeakPtr(), binding_context, options,
                           std::move(callback)),
            base::CreateSingleThreadTaskRunner({BrowserThread::IO})));
    return;
  }

  if (options.type() ==
      blink::mojom::ChooseFileSystemEntryType::kOpenDirectory) {
    DCHECK_EQ(entries.size(), 1u);
    if (permission_context_) {
      permission_context_->ConfirmDirectoryReadAccess(
          binding_context.origin, entries.front(), binding_context.process_id,
          binding_context.frame_id,
          base::BindOnce(&NativeFileSystemManagerImpl::DidChooseDirectory, this,
                         binding_context, entries.front(),
                         std::move(callback)));
    } else {
      DidChooseDirectory(binding_context, entries.front(), std::move(callback),
                         PermissionStatus::GRANTED);
    }
    return;
  }

  if (options.type() == blink::mojom::ChooseFileSystemEntryType::kSaveFile) {
    DCHECK_EQ(entries.size(), 1u);
    // Create file if it doesn't yet exist, and truncate file if it does exist.
    base::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::ThreadPool(), base::TaskPriority::USER_BLOCKING,
         base::MayBlock()},
        base::BindOnce(&CreateOrTruncateFile, entries.front()),
        base::BindOnce(
            &NativeFileSystemManagerImpl::DidCreateOrTruncateSaveFile, this,
            binding_context, entries.front(), std::move(callback)));
    return;
  }

  std::vector<blink::mojom::NativeFileSystemEntryPtr> result_entries;
  result_entries.reserve(entries.size());
  for (const auto& entry : entries)
    result_entries.push_back(CreateFileEntryFromPath(binding_context, entry));
  std::move(callback).Run(native_file_system_error::Ok(),
                          std::move(result_entries));
}

void NativeFileSystemManagerImpl::DidCreateOrTruncateSaveFile(
    const BindingContext& binding_context,
    const base::FilePath& path,
    ChooseEntriesCallback callback,
    bool success) {
  std::vector<blink::mojom::NativeFileSystemEntryPtr> result_entries;
  if (!success) {
    std::move(callback).Run(
        native_file_system_error::FromStatus(
            blink::mojom::NativeFileSystemStatus::kOperationFailed,
            "Failed to create or truncate file"),
        std::move(result_entries));
    return;
  }
  result_entries.push_back(
      CreateWritableFileEntryFromPath(binding_context, path));
  std::move(callback).Run(native_file_system_error::Ok(),
                          std::move(result_entries));
}

void NativeFileSystemManagerImpl::DidChooseDirectory(
    const BindingContext& binding_context,
    const base::FilePath& path,
    ChooseEntriesCallback callback,
    NativeFileSystemPermissionContext::PermissionStatus permission) {
  base::UmaHistogramEnumeration(
      "NativeFileSystemAPI.ConfirmReadDirectoryResult", permission);

  std::vector<blink::mojom::NativeFileSystemEntryPtr> result_entries;
  if (permission != PermissionStatus::GRANTED) {
    std::move(callback).Run(native_file_system_error::FromStatus(
                                NativeFileSystemStatus::kOperationAborted),
                            std::move(result_entries));
    return;
  }

  result_entries.push_back(CreateDirectoryEntryFromPath(binding_context, path));
  std::move(callback).Run(native_file_system_error::Ok(),
                          std::move(result_entries));
}

void NativeFileSystemManagerImpl::CreateTransferTokenImpl(
    const storage::FileSystemURL& url,
    const SharedHandleState& handle_state,
    bool is_directory,
    mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken>
        receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto token_impl = std::make_unique<NativeFileSystemTransferTokenImpl>(
      url, handle_state,
      is_directory ? NativeFileSystemTransferTokenImpl::HandleType::kDirectory
                   : NativeFileSystemTransferTokenImpl::HandleType::kFile,
      this, std::move(receiver));
  auto token = token_impl->token();
  transfer_tokens_.emplace(token, std::move(token_impl));
}

void NativeFileSystemManagerImpl::RemoveToken(
    const base::UnguessableToken& token) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  size_t count_removed = transfer_tokens_.erase(token);
  DCHECK_EQ(1u, count_removed);
}

void NativeFileSystemManagerImpl::DoResolveTransferToken(
    mojo::Remote<blink::mojom::NativeFileSystemTransferToken>,
    ResolvedTokenCallback callback,
    const base::UnguessableToken& token) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto it = transfer_tokens_.find(token);
  if (it == transfer_tokens_.end()) {
    std::move(callback).Run(nullptr);
  } else {
    std::move(callback).Run(it->second.get());
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

blink::mojom::NativeFileSystemEntryPtr
NativeFileSystemManagerImpl::CreateFileEntryFromPathImpl(
    const BindingContext& binding_context,
    const base::FilePath& file_path,
    NativeFileSystemPermissionContext::UserAction user_action) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto url = CreateFileSystemURLFromPath(binding_context.origin, file_path);

  scoped_refptr<NativeFileSystemPermissionGrant> read_grant, write_grant;
  if (permission_context_) {
    read_grant = permission_context_->GetReadPermissionGrant(
        binding_context.origin, file_path, /*is_directory=*/false,
        binding_context.process_id, binding_context.frame_id);
    write_grant = permission_context_->GetWritePermissionGrant(
        binding_context.origin, file_path, /*is_directory=*/false,
        binding_context.process_id, binding_context.frame_id, user_action);
  } else {
    // Grant read permission even without a permission_context_, as the picker
    // itself is enough UI to assume user intent.
    read_grant = base::MakeRefCounted<FixedNativeFileSystemPermissionGrant>(
        PermissionStatus::GRANTED);
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
      blink::mojom::NativeFileSystemHandle::NewFile(CreateFileHandle(
          binding_context, url.url,
          SharedHandleState(std::move(read_grant), std::move(write_grant),
                            std::move(url.file_system)))),
      url.base_name);
}

void NativeFileSystemManagerImpl::CreateFileWriterImpl(
    const BindingContext& binding_context,
    const storage::FileSystemURL& url,
    const storage::FileSystemURL& swap_url,
    const SharedHandleState& handle_state,
    mojo::PendingReceiver<blink::mojom::NativeFileSystemFileWriter>
        writer_receiver,
    bool has_transient_user_activation) {
  writer_receivers_.Add(std::make_unique<NativeFileSystemFileWriterImpl>(
                            this, binding_context, url, swap_url, handle_state,
                            has_transient_user_activation),
                        std::move(writer_receiver));
}

}  // namespace content
