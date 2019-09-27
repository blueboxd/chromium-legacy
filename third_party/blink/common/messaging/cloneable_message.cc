// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/messaging/cloneable_message.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/messaging/cloneable_message.mojom.h"

namespace blink {

CloneableMessage::CloneableMessage() = default;
CloneableMessage::CloneableMessage(CloneableMessage&&) = default;
CloneableMessage& CloneableMessage::operator=(CloneableMessage&&) = default;
CloneableMessage::~CloneableMessage() = default;

CloneableMessage CloneableMessage::ShallowClone() const {
  CloneableMessage clone;
  clone.encoded_message = encoded_message;
  for (const auto& blob : blobs) {
    // NOTE: We dubiously exercise dual ownership of the blob's pipe handle here
    // so that we can temporarily bind and send a message over the pipe without
    // mutating the state of this CloneableMessage.
    mojo::ScopedMessagePipeHandle handle(blob->blob.Pipe().get());
    mojo::Remote<mojom::Blob> blob_proxy(mojo::PendingRemote<mojom::Blob>(
        std::move(handle), blob->blob.version()));
    mojo::PendingRemote<mojom::Blob> blob_clone_remote;
    blob_proxy->Clone(blob_clone_remote.InitWithNewPipeAndPassReceiver());
    clone.blobs.push_back(
        mojom::SerializedBlob::New(blob->uuid, blob->content_type, blob->size,
                                   std::move(blob_clone_remote)));

    // Not leaked - still owned by |blob->blob|.
    ignore_result(blob_proxy.Unbind().PassPipe().release());
  }
  return clone;
}

void CloneableMessage::EnsureDataIsOwned() {
  if (encoded_message.data() == owned_encoded_message.data())
    return;
  owned_encoded_message.assign(encoded_message.begin(), encoded_message.end());
  encoded_message = owned_encoded_message;
}

}  // namespace blink
