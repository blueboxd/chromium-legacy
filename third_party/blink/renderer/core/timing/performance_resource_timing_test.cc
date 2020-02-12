// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_resource_timing.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class PerformanceResourceTimingTest : public testing::Test {
 protected:
  AtomicString GetNextHopProtocol(const AtomicString& alpn_negotiated_protocol,
                                  const AtomicString& connection_info) {
    mojom::blink::ResourceTimingInfo info;
    info.allow_timing_details = true;
    PerformanceResourceTiming timing(
        info, base::TimeTicks(), /*initiator_type=*/"",
        mojo::PendingReceiver<mojom::blink::WorkerTimingContainer>());
    return timing.GetNextHopProtocol(alpn_negotiated_protocol, connection_info);
  }
};

TEST_F(PerformanceResourceTimingTest,
       TestFallbackToConnectionInfoWhenALPNUnknown) {
  AtomicString connection_info = "http/1.1";
  AtomicString alpn_negotiated_protocol = "unknown";
  EXPECT_EQ(GetNextHopProtocol(alpn_negotiated_protocol, connection_info),
            connection_info);
}

TEST_F(PerformanceResourceTimingTest,
       TestFallbackToHTTPInfoWhenALPNAndConnectionInfoUnknown) {
  AtomicString connection_info = "unknown";
  AtomicString alpn_negotiated_protocol = "unknown";
  EXPECT_EQ(GetNextHopProtocol(alpn_negotiated_protocol, connection_info), "");
}

TEST_F(PerformanceResourceTimingTest, TestNoChangeWhenContainsQuic) {
  AtomicString connection_info = "http/1.1";
  AtomicString alpn_negotiated_protocol = "http/2+quic/39";
  EXPECT_EQ(GetNextHopProtocol(alpn_negotiated_protocol, connection_info),
            alpn_negotiated_protocol);
}

TEST_F(PerformanceResourceTimingTest, TestNoChangeWhenOtherwise) {
  AtomicString connection_info = "http/1.1";
  AtomicString alpn_negotiated_protocol = "RandomProtocol";
  EXPECT_EQ(GetNextHopProtocol(alpn_negotiated_protocol, connection_info),
            alpn_negotiated_protocol);
}
}  // namespace blink
