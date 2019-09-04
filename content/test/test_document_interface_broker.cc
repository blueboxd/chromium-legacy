// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_document_interface_broker.h"

namespace content {

TestDocumentInterfaceBroker::TestDocumentInterfaceBroker(
    blink::mojom::DocumentInterfaceBroker* document_interface_broker,
    mojo::PendingReceiver<blink::mojom::DocumentInterfaceBroker> receiver)
    : real_broker_(document_interface_broker),
      receiver_(this, std::move(receiver)) {}

TestDocumentInterfaceBroker::~TestDocumentInterfaceBroker() {}

blink::mojom::DocumentInterfaceBroker*
TestDocumentInterfaceBroker::GetForwardingInterface() {
  return real_broker_;
}

void TestDocumentInterfaceBroker::Flush() {
  receiver_.FlushForTesting();
}

}  // namespace content