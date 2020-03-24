// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_MOCK_WEB_TRANSPORT_CLOSE_PROXY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_MOCK_WEB_TRANSPORT_CLOSE_PROXY_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport_close_proxy.h"

namespace blink {

// A mock implementation of WebTransportCloseProxy.
class MockWebTransportCloseProxy : public WebTransportCloseProxy {
 public:
  // Constructor and destructor are out-of-line to reduce compile time:
  // https://github.com/google/googletest/blob/master/googlemock/docs/cook_book.md#making-the-compilation-faster.
  MockWebTransportCloseProxy();
  ~MockWebTransportCloseProxy() override;

  MOCK_METHOD1(OnIncomingStreamClosed, void(bool));
  MOCK_METHOD0(SendFin, void());
  MOCK_METHOD0(ForgetStream, void());
  MOCK_METHOD0(Reset, void());
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_MOCK_WEB_TRANSPORT_CLOSE_PROXY_H_
