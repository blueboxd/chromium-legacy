// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_file_system/native_file_system_transfer_token_impl.h"

namespace content {

NativeFileSystemTransferTokenImpl::NativeFileSystemTransferTokenImpl(
    const storage::FileSystemURL& url,
    const SharedHandleState& handle_state,
    HandleType type)
    : token_(base::UnguessableToken::Create()),
      url_(url),
      handle_state_(handle_state),
      type_(type) {
  DCHECK_EQ(url_.mount_type() == storage::kFileSystemTypeIsolated,
            handle_state_.file_system.is_valid())
      << url_.mount_type();
}

void NativeFileSystemTransferTokenImpl::GetInternalID(
    GetInternalIDCallback callback) {
  std::move(callback).Run(token_);
}

}  // namespace content
