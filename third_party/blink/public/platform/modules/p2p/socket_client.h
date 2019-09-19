// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_P2P_SOCKET_CLIENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_P2P_SOCKET_CLIENT_H_

#include <stdint.h>

#include <vector>

#include "net/base/ip_endpoint.h"
#include "services/network/public/cpp/p2p_socket_type.h"

namespace rtc {
struct PacketOptions;
}

namespace blink {

class P2PSocketClientDelegate;

// P2P socket that routes all calls over IPC.
//
// TODO(crbug.com/787254): Move this class out of the Blink exposed API when
// all users of it have been Onion souped. Also, move it away from std::vector.
class P2PSocketClient {
 public:
  virtual ~P2PSocketClient() {}

  // Send the |data| to the |address| using Differentiated Services Code Point
  // |dscp|. Return value is the unique packet_id for this packet.
  virtual uint64_t Send(const net::IPEndPoint& address,
                        const std::vector<int8_t>& data,
                        const rtc::PacketOptions& options) = 0;

  virtual void SetOption(network::P2PSocketOption option, int value) = 0;

  // Must be called before the socket is destroyed.
  virtual void Close() = 0;

  virtual int GetSocketID() const = 0;
  virtual void SetDelegate(P2PSocketClientDelegate* delegate) = 0;

 protected:
  P2PSocketClient() {}
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_P2P_SOCKET_CLIENT_H_
