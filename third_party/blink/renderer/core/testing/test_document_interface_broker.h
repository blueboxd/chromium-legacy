// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_TEST_DOCUMENT_INTERFACE_BROKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_TEST_DOCUMENT_INTERFACE_BROKER_H_

#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/frame/document_interface_broker.mojom-blink-test-utils.h"

namespace blink {

// This class can be subclassed to override specific methods of
// LocalFrameClient's DocumentInterfaceBroker in tests. The rest of the calls
// will be forwarded to the implementation passed to the constructor (typically
// returned by LocalFrameClient::GetDocumentInterfaceBroker()).
class TestDocumentInterfaceBroker
    : public mojom::blink::DocumentInterfaceBrokerInterceptorForTesting {
 public:
  TestDocumentInterfaceBroker(
      mojom::blink::DocumentInterfaceBroker* document_interface_broker,
      mojo::PendingReceiver<mojom::blink::DocumentInterfaceBroker> receiver);
  ~TestDocumentInterfaceBroker() override;
  mojom::blink::DocumentInterfaceBroker* GetForwardingInterface() override;
  void Flush();

 private:
  mojom::blink::DocumentInterfaceBroker* real_broker_;
  mojo::Receiver<mojom::blink::DocumentInterfaceBroker> receiver_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_TEST_DOCUMENT_INTERFACE_BROKER_H_
