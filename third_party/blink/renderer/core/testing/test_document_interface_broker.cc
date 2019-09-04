// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/test_document_interface_broker.h"

#include <utility>

namespace blink {

TestDocumentInterfaceBroker::TestDocumentInterfaceBroker(
    mojom::blink::DocumentInterfaceBroker* document_interface_broker,
    mojo::PendingReceiver<mojom::blink::DocumentInterfaceBroker> receiver)
    : real_broker_(document_interface_broker),
      receiver_(this, std::move(receiver)) {}

TestDocumentInterfaceBroker::~TestDocumentInterfaceBroker() {}

mojom::blink::DocumentInterfaceBroker*
TestDocumentInterfaceBroker::GetForwardingInterface() {
  return real_broker_;
}

void TestDocumentInterfaceBroker::Flush() {
  receiver_.FlushForTesting();
}

}  // namespace blink
