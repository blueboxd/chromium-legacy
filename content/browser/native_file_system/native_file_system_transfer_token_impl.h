// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_TRANSFER_TOKEN_IMPL_H_
#define CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_TRANSFER_TOKEN_IMPL_H_

#include "content/browser/native_file_system/native_file_system_manager_impl.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/isolated_context.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_transfer_token.mojom.h"

namespace content {

// Base class for blink::mojom::NativeFileSystemTransferToken implementations.
//
// Instances of this class should always be used from the sequence they were
// created on.
class CONTENT_EXPORT NativeFileSystemTransferTokenImpl
    : public blink::mojom::NativeFileSystemTransferToken {
 public:
  enum class HandleType { kFile, kDirectory };

  // Create a token that is tied to a particular origin (the origin of |url|,
  // and uses the permission grants in |handle_state| when creating new handles
  // out of the token. This is used for postMessage and IndexedDB serialization,
  // as well as a couple of other APIs.
  static std::unique_ptr<NativeFileSystemTransferTokenImpl> Create(
      const storage::FileSystemURL& url,
      const NativeFileSystemManagerImpl::SharedHandleState& handle_state,
      HandleType type,
      NativeFileSystemManagerImpl* manager,
      mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken>
          receiver);

  NativeFileSystemTransferTokenImpl(
      HandleType type,
      NativeFileSystemManagerImpl* manager,
      mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken>
          receiver);
  ~NativeFileSystemTransferTokenImpl() override;

  const base::UnguessableToken& token() const { return token_; }
  HandleType type() const { return type_; }

  // Returns true if |origin| is allowed to use this token.
  virtual bool MatchesOrigin(const url::Origin& origin) const = 0;

  // Can return nullptr if this token isn't represented by a FileSystemURL.
  virtual const storage::FileSystemURL* GetAsFileSystemURL() const = 0;

  // Returns permission grants associated with this token. These can
  // return nullptr if this token does not have associated permission grants.
  virtual NativeFileSystemPermissionGrant* GetReadGrant() const = 0;
  virtual NativeFileSystemPermissionGrant* GetWriteGrant() const = 0;

  virtual std::unique_ptr<NativeFileSystemFileHandleImpl> CreateFileHandle(
      const NativeFileSystemManagerImpl::BindingContext& binding_context) = 0;
  virtual std::unique_ptr<NativeFileSystemDirectoryHandleImpl>
  CreateDirectoryHandle(
      const NativeFileSystemManagerImpl::BindingContext& binding_context) = 0;

  // blink::mojom::NativeFileSystemTransferToken:
  void GetInternalID(GetInternalIDCallback callback) override;
  void Clone(mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken>
                 clone_receiver) override;

 protected:
  const base::UnguessableToken token_;
  const HandleType type_;
  // Raw pointer since NativeFileSystemManagerImpl owns |this|.
  NativeFileSystemManagerImpl* const manager_;

 private:
  void OnMojoDisconnect();

  // This token may contain multiple receivers, which includes a receiver for
  // the originally constructed instance and then additional receivers for
  // each clone. |manager_| must not remove this token until |receivers_| is
  // empty.
  mojo::ReceiverSet<blink::mojom::NativeFileSystemTransferToken> receivers_;

  DISALLOW_COPY_AND_ASSIGN(NativeFileSystemTransferTokenImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_TRANSFER_TOKEN_IMPL_H_
