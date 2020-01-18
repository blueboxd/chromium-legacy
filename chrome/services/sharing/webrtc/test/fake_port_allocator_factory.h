// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_WEBRTC_TEST_FAKE_PORT_ALLOCATOR_FACTORY_H_
#define CHROME_SERVICES_SHARING_WEBRTC_TEST_FAKE_PORT_ALLOCATOR_FACTORY_H_

#include <memory>

#include "third_party/webrtc/p2p/base/port_allocator.h"
#include "third_party/webrtc/rtc_base/network.h"

namespace sharing {

class FakePacketSocketFactory;

class FakePortAllocatorFactory {
 public:
  FakePortAllocatorFactory();
  FakePortAllocatorFactory(const FakePortAllocatorFactory&) = delete;
  FakePortAllocatorFactory& operator=(const FakePortAllocatorFactory&) = delete;
  ~FakePortAllocatorFactory();

  std::unique_ptr<cricket::PortAllocator> CreatePortAllocator();

 private:
  std::unique_ptr<rtc::NetworkManager> network_manager_;
  std::unique_ptr<FakePacketSocketFactory> socket_factory_;
};

}  // namespace sharing

#endif  // CHROME_SERVICES_SHARING_WEBRTC_TEST_FAKE_PORT_ALLOCATOR_FACTORY_H_
