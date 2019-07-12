// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/document_interface_broker_test_helpers.h"

#include <utility>

namespace blink {

void FrameHostTestInterfaceImpl::BindAndFlush(
    mojo::PendingReceiver<mojom::blink::FrameHostTestInterface> receiver) {
  receiver_.Bind(std::move(receiver));
  receiver_.WaitForIncomingCall();
}

void FrameHostTestInterfaceImpl::GetName(GetNameCallback callback) {
  std::move(callback).Run(kGetNameTestResponse);
}

void FrameHostTestDocumentInterfaceBroker::GetFrameHostTestInterface(
    mojo::PendingReceiver<mojom::blink::FrameHostTestInterface> receiver) {
  FrameHostTestInterfaceImpl impl;
  impl.BindAndFlush(std::move(receiver));
}

}  // namespace blink
