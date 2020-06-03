// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_file_system/native_file_system_transfer_token_impl.h"

#include "content/browser/native_file_system/native_file_system_directory_handle_impl.h"
#include "content/browser/native_file_system/native_file_system_file_handle_impl.h"

namespace content {

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
      HandleType type,
      NativeFileSystemManagerImpl* manager,
      mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken>
          receiver)
      : NativeFileSystemTransferTokenImpl(type, manager, std::move(receiver)),
        url_(url),
        handle_state_(handle_state) {
    DCHECK_EQ(url_.mount_type() == storage::kFileSystemTypeIsolated,
              handle_state_.file_system.is_valid())
        << url_.mount_type();
  }
  ~NativeFileSystemTransferTokenImplForHandles() override = default;

  bool MatchesOrigin(const url::Origin& origin) const override {
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
    DCHECK_EQ(type_, HandleType::kFile);
    return std::make_unique<NativeFileSystemFileHandleImpl>(
        manager_, binding_context, url_, handle_state_);
  }

  std::unique_ptr<NativeFileSystemDirectoryHandleImpl> CreateDirectoryHandle(
      const NativeFileSystemManagerImpl::BindingContext& binding_context)
      override {
    DCHECK_EQ(type_, HandleType::kDirectory);
    return std::make_unique<NativeFileSystemDirectoryHandleImpl>(
        manager_, binding_context, url_, handle_state_);
  }

 private:
  const storage::FileSystemURL url_;
  const NativeFileSystemManagerImpl::SharedHandleState handle_state_;
};

}  // namespace

// static
std::unique_ptr<NativeFileSystemTransferTokenImpl>
NativeFileSystemTransferTokenImpl::Create(
    const storage::FileSystemURL& url,
    const NativeFileSystemManagerImpl::SharedHandleState& handle_state,
    HandleType type,
    NativeFileSystemManagerImpl* manager,
    mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken>
        receiver) {
  return std::make_unique<NativeFileSystemTransferTokenImplForHandles>(
      url, handle_state, type, manager, std::move(receiver));
}

NativeFileSystemTransferTokenImpl::NativeFileSystemTransferTokenImpl(
    HandleType type,
    NativeFileSystemManagerImpl* manager,
    mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken> receiver)
    : token_(base::UnguessableToken::Create()), type_(type), manager_(manager) {
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
