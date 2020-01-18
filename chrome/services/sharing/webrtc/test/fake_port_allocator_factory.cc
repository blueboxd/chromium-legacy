// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/webrtc/test/fake_port_allocator_factory.h"

#include "chrome/services/sharing/webrtc/test/fake_network_manager.h"
#include "chrome/services/sharing/webrtc/test/fake_packet_socket_factory.h"
#include "third_party/webrtc/p2p/client/basic_port_allocator.h"

namespace sharing {

FakePortAllocatorFactory::FakePortAllocatorFactory() {
  rtc::IPAddress address(INADDR_LOOPBACK);
  socket_factory_ = std::make_unique<FakePacketSocketFactory>(address);
  network_manager_ = std::make_unique<FakeNetworkManager>(address);
}

FakePortAllocatorFactory::~FakePortAllocatorFactory() = default;

std::unique_ptr<cricket::PortAllocator>
FakePortAllocatorFactory::CreatePortAllocator() {
  auto allocator = std::make_unique<cricket::BasicPortAllocator>(
      network_manager_.get(), socket_factory_.get());
  allocator->set_flags(cricket::PORTALLOCATOR_DISABLE_TCP |
                       cricket::PORTALLOCATOR_ENABLE_IPV6 |
                       cricket::PORTALLOCATOR_DISABLE_STUN |
                       cricket::PORTALLOCATOR_DISABLE_RELAY);
  allocator->Initialize();
  return allocator;
}

}  // namespace sharing
