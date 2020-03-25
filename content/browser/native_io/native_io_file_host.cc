// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_io/native_io_file_host.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/sequence_checker.h"
#include "content/browser/native_io/native_io_host.h"
#include "third_party/blink/public/mojom/native_io/native_io.mojom.h"

namespace content {

NativeIOFileHost::NativeIOFileHost(
    mojo::PendingReceiver<blink::mojom::NativeIOFileHost> file_host_receiver,
    NativeIOHost* origin_host,
    std::string file_name)
    : origin_host_(origin_host),
      receiver_(this, std::move(file_host_receiver)),
      file_name_(std::move(file_name)) {
  // base::Unretained is safe here because this NativeIOFileHost owns
  // |receiver_|. So, the unretained NativeIOFileHost is guaranteed to outlive
  // |receiver_| and the closure that it uses.
  receiver_.set_disconnect_handler(base::BindRepeating(
      &NativeIOFileHost::OnReceiverDisconnect, base::Unretained(this)));
}

NativeIOFileHost::~NativeIOFileHost() = default;

void NativeIOFileHost::Close(CloseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::move(callback).Run();
  origin_host_->OnFileClose(this);  // Deletes |this|.
}

void NativeIOFileHost::OnReceiverDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  origin_host_->OnFileClose(this);  // Deletes |this|.
}

}  // namespace content
