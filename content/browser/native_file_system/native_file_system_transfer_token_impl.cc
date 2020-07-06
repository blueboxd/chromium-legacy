// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_file_system/native_file_system_transfer_token_impl.h"

#include "content/browser/native_file_system/native_file_system_directory_handle_impl.h"
#include "content/browser/native_file_system/native_file_system_file_handle_impl.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_directory_handle.mojom.h"

namespace content {
using HandleType = NativeFileSystemPermissionContext::HandleType;

namespace {

// Concrete implementation for Transfer Tokens created from a
// NativeFileSystemFileHandleImpl or DirectoryHandleImpl. These tokens
// share permission grants with the handle, and are tied to the origin the
// handles were associated with.
class NativeFileSystemTransferTokenImplForHandles
    : public NativeFileSystemTransferTokenImpl {
 public:
  using SharedHandleState = NativeFileSystemManagerImpl::SharedHandleState;

  NativeFileSystemTransferTokenImplForHandles(
      const storage::FileSystemURL& url,
      const NativeFileSystemManagerImpl::SharedHandleState& handle_state,
      HandleType handle_type,
      NativeFileSystemManagerImpl* manager,
      mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken>
          receiver)
      : NativeFileSystemTransferTokenImpl(handle_type,
                                          manager,
                                          std::move(receiver)),
        url_(url),
        handle_state_(handle_state) {
    DCHECK_EQ(url_.mount_type() == storage::kFileSystemTypeIsolated,
              handle_state_.file_system.is_valid())
        << url_.mount_type();
  }
  ~NativeFileSystemTransferTokenImplForHandles() override = default;

  bool MatchesOriginAndPID(const url::Origin& origin,
                           int process_id) const override {
    return url_.origin() == origin;
  }

  const storage::FileSystemURL* GetAsFileSystemURL() const override {
    return &url_;
  }

  NativeFileSystemPermissionGrant* GetReadGrant() const override {
    return handle_state_.read_grant.get();
  }

  NativeFileSystemPermissionGrant* GetWriteGrant() const override {
    return handle_state_.write_grant.get();
  }

  std::unique_ptr<NativeFileSystemFileHandleImpl> CreateFileHandle(
      const NativeFileSystemManagerImpl::BindingContext& binding_context)
      override {
    DCHECK_EQ(handle_type_, HandleType::kFile);
    return std::make_unique<NativeFileSystemFileHandleImpl>(
        manager_, binding_context, url_, handle_state_);
  }

  std::unique_ptr<NativeFileSystemDirectoryHandleImpl> CreateDirectoryHandle(
      const NativeFileSystemManagerImpl::BindingContext& binding_context)
      override {
    DCHECK_EQ(handle_type_, HandleType::kDirectory);
    return std::make_unique<NativeFileSystemDirectoryHandleImpl>(
        manager_, binding_context, url_, handle_state_);
  }

 private:
  const storage::FileSystemURL url_;
  const NativeFileSystemManagerImpl::SharedHandleState handle_state_;
};

// Concrete implementation for Transfer Tokens created with
// a base::FilePath and no associated origin or implementation at the time of.
// creation. These tokens serve as a wrapper around |file_path| and can be
// passed between processes. Used for transferring dropped file information
// between browser and renderer processes during drag and drop operations.
class NativeFileSystemTransferTokenFromPath
    : public NativeFileSystemTransferTokenImpl {
 public:
  NativeFileSystemTransferTokenFromPath(
      const base::FilePath& file_path,
      HandleType type,
      NativeFileSystemManagerImpl* manager,
      mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken>
          receiver,
      int renderer_process_id)
      : NativeFileSystemTransferTokenImpl(type, manager, std::move(receiver)),
        file_path_(file_path),
        renderer_process_id_(renderer_process_id) {}

  ~NativeFileSystemTransferTokenFromPath() override = default;

  bool MatchesOriginAndPID(const url::Origin& origin,
                           int process_id) const override {
    return renderer_process_id_ == process_id;
  }

  const storage::FileSystemURL* GetAsFileSystemURL() const override {
    return nullptr;
  }

  NativeFileSystemPermissionGrant* GetReadGrant() const override {
    return nullptr;
  }

  NativeFileSystemPermissionGrant* GetWriteGrant() const override {
    return nullptr;
  }

  std::unique_ptr<NativeFileSystemFileHandleImpl> CreateFileHandle(
      const NativeFileSystemManagerImpl::BindingContext& binding_context)
      override {
    DCHECK_EQ(handle_type_, HandleType::kFile);
    NativeFileSystemManagerImpl::FileSystemURLAndFSHandle url =
        manager_->CreateFileSystemURLFromPath(binding_context.origin,
                                              file_path_);
    NativeFileSystemManagerImpl::SharedHandleState shared_handle_state =
        manager_->GetSharedHandleStateForPath(
            file_path_, binding_context.origin, std::move(url.file_system),
            HandleType::kFile,
            NativeFileSystemPermissionContext::UserAction::kOpen);
    return std::make_unique<NativeFileSystemFileHandleImpl>(
        manager_, binding_context, url.url, shared_handle_state);
  }

  std::unique_ptr<NativeFileSystemDirectoryHandleImpl> CreateDirectoryHandle(
      const NativeFileSystemManagerImpl::BindingContext& binding_context)
      override {
    DCHECK_EQ(handle_type_, HandleType::kDirectory);
    NativeFileSystemManagerImpl::FileSystemURLAndFSHandle url =
        manager_->CreateFileSystemURLFromPath(binding_context.origin,
                                              file_path_);
    NativeFileSystemManagerImpl::SharedHandleState shared_handle_state =
        manager_->GetSharedHandleStateForPath(
            file_path_, binding_context.origin, url.file_system,
            HandleType::kDirectory,
            NativeFileSystemPermissionContext::UserAction::kOpen);
    return std::make_unique<NativeFileSystemDirectoryHandleImpl>(
        manager_, binding_context, url.url, shared_handle_state);
  }

 private:
  const base::FilePath file_path_;
  const int renderer_process_id_;
};
}  // namespace

// static
std::unique_ptr<NativeFileSystemTransferTokenImpl>
NativeFileSystemTransferTokenImpl::Create(
    const storage::FileSystemURL& url,
    const NativeFileSystemManagerImpl::SharedHandleState& handle_state,
    HandleType handle_type,
    NativeFileSystemManagerImpl* manager,
    mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken>
        receiver) {
  return std::make_unique<NativeFileSystemTransferTokenImplForHandles>(
      url, handle_state, handle_type, manager, std::move(receiver));
}

// static
std::unique_ptr<NativeFileSystemTransferTokenImpl>
NativeFileSystemTransferTokenImpl::CreateFromPath(
    const base::FilePath file_path,
    HandleType handle_type,
    NativeFileSystemManagerImpl* manager,
    mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken> receiver,
    int renderer_process_id) {
  return std::make_unique<NativeFileSystemTransferTokenFromPath>(
      file_path, handle_type, manager, std::move(receiver),
      renderer_process_id);
}

NativeFileSystemTransferTokenImpl::NativeFileSystemTransferTokenImpl(
    HandleType handle_type,
    NativeFileSystemManagerImpl* manager,
    mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken> receiver)
    : token_(base::UnguessableToken::Create()),
      handle_type_(handle_type),
      manager_(manager) {
  DCHECK(manager_);

  receivers_.set_disconnect_handler(
      base::BindRepeating(&NativeFileSystemTransferTokenImpl::OnMojoDisconnect,
                          base::Unretained(this)));

  receivers_.Add(this, std::move(receiver));
}

NativeFileSystemTransferTokenImpl::~NativeFileSystemTransferTokenImpl() =
    default;

void NativeFileSystemTransferTokenImpl::GetInternalID(
    GetInternalIDCallback callback) {
  std::move(callback).Run(token_);
}

void NativeFileSystemTransferTokenImpl::OnMojoDisconnect() {
  if (receivers_.empty()) {
    manager_->RemoveToken(token_);
  }
}

void NativeFileSystemTransferTokenImpl::Clone(
    mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken>
        clone_receiver) {
  receivers_.Add(this, std::move(clone_receiver));
}

}  // namespace content
