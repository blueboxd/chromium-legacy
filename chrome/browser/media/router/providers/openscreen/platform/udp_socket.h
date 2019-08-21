// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_PLATFORM_UDP_SOCKET_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_PLATFORM_UDP_SOCKET_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "services/network/udp_socket.h"
#include "third_party/openscreen/src/platform/api/udp_socket.h"
#include "third_party/openscreen/src/platform/base/error.h"

namespace net {
class IPEndPoint;
}

namespace openscreen {
namespace platform {

struct ChromeUdpSocket : public UdpSocket {
 public:
  ChromeUdpSocket(std::unique_ptr<network::UDPSocket> udp_socket,
                  TaskRunner* task_runner,
                  Client* client,
                  const IPEndpoint& local_endpoint);
  ~ChromeUdpSocket() final;

  ErrorOr<UdpPacket> ReceiveMessage();

  // Implementations of UdpSocket methods.
  bool IsIPv4() const final;
  bool IsIPv6() const final;
  Error Bind() final;
  Error SetMulticastOutboundInterface(NetworkInterfaceIndex ifindex) final;
  Error JoinMulticastGroup(const IPAddress& address,
                           NetworkInterfaceIndex ifindex) final;
  Error SendMessage(const void* data,
                    size_t length,
                    const IPEndpoint& dest) final;
  Error SetDscp(DscpMode state) final;

  // Method that allows for getting the last async error, set by internal
  // callback methods.
  int32_t GetLastError();

 private:
  void BindCallback(int32_t result,
                    const base::Optional<net::IPEndPoint>& address);
  void JoinGroupCallback(int32_t result);
  void SendToCallback(int32_t result);

  bool is_bound_ = false;
  int32_t last_error_ = 0;
  const net::IPEndPoint local_endpoint_;
  std::unique_ptr<network::UDPSocket> udp_socket_;
  base::WeakPtrFactory<ChromeUdpSocket> weak_ptr_factory_{this};
};

}  // namespace platform
}  // namespace openscreen

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_PLATFORM_UDP_SOCKET_H_
