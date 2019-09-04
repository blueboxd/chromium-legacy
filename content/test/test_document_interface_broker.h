// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_DOCUMENT_INTERFACE_BROKER_H_
#define CONTENT_TEST_TEST_DOCUMENT_INTERFACE_BROKER_H_

#include <utility>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/frame/document_interface_broker.mojom-test-utils.h"

namespace content {
// This class can be subclassed to override specific methods of RenderFrame's
// DocumentInterfaceBroker in tests. The rest of the calls will be forwarded to
// the implementation passed to the constructor (typically returned by
// RenderFrame::GetDocumentInterfaceBroker()).
class TestDocumentInterfaceBroker
    : public blink::mojom::DocumentInterfaceBrokerInterceptorForTesting {
 public:
  TestDocumentInterfaceBroker(
      blink::mojom::DocumentInterfaceBroker* document_interface_broker,
      mojo::PendingReceiver<blink::mojom::DocumentInterfaceBroker> receiver);
  ~TestDocumentInterfaceBroker() override;
  blink::mojom::DocumentInterfaceBroker* GetForwardingInterface() override;
  void Flush();

 private:
  blink::mojom::DocumentInterfaceBroker* real_broker_;
  mojo::Receiver<DocumentInterfaceBroker> receiver_;
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_DOCUMENT_INTERFACE_BROKER_H_