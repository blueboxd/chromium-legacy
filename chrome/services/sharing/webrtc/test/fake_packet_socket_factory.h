// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_WEBRTC_TEST_FAKE_PACKET_SOCKET_FACTORY_H_
#define CHROME_SERVICES_SHARING_WEBRTC_TEST_FAKE_PACKET_SOCKET_FACTORY_H_

#include <stdint.h>

#include <list>
#include <map>
#include <memory>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "third_party/webrtc/api/packet_socket_factory.h"
#include "third_party/webrtc/rtc_base/ip_address.h"
#include "third_party/webrtc/rtc_base/socket_address.h"

namespace net {
class IOBuffer;
}  // namespace net

namespace sharing {

class FakePacketSocketFactory : public rtc::PacketSocketFactory {
 public:
  explicit FakePacketSocketFactory(const rtc::IPAddress& address);
  FakePacketSocketFactory(const FakePacketSocketFactory&) = delete;
  FakePacketSocketFactory& operator=(const FakePacketSocketFactory&) = delete;
  ~FakePacketSocketFactory() override;

  void OnSocketDestroyed(int port);

  // rtc::PacketSocketFactory interface.
  rtc::AsyncPacketSocket* CreateUdpSocket(
      const rtc::SocketAddress& local_address,
      uint16_t min_port,
      uint16_t max_port) override;
  rtc::AsyncPacketSocket* CreateServerTcpSocket(
      const rtc::SocketAddress& local_address,
      uint16_t min_port,
      uint16_t max_port,
      int opts) override;
  rtc::AsyncPacketSocket* CreateClientTcpSocket(
      const rtc::SocketAddress& local_address,
      const rtc::SocketAddress& remote_address,
      const rtc::ProxyInfo& proxy_info,
      const std::string& user_agent,
      const rtc::PacketSocketTcpOptions& opts) override;
  rtc::AsyncResolverInterface* CreateAsyncResolver() override;

  void ReceivePacket(const rtc::SocketAddress& from,
                     const rtc::SocketAddress& to,
                     const scoped_refptr<net::IOBuffer>& data,
                     int data_size);

 private:
  struct PendingPacket {
    PendingPacket();
    PendingPacket(const rtc::SocketAddress& from,
                  const rtc::SocketAddress& to,
                  const scoped_refptr<net::IOBuffer>& data,
                  int data_size);
    PendingPacket(const PendingPacket& other);
    ~PendingPacket();

    rtc::SocketAddress from;
    rtc::SocketAddress to;
    scoped_refptr<net::IOBuffer> data;
    int data_size;
  };

  typedef base::Callback<void(const rtc::SocketAddress& from,
                              const rtc::SocketAddress& to,
                              const scoped_refptr<net::IOBuffer>& data,
                              int data_size)>
      ReceiveCallback;
  typedef std::map<uint16_t, ReceiveCallback> UdpSocketsMap;

  void DoReceivePacket();

  rtc::IPAddress address_;

  UdpSocketsMap udp_sockets_;
  uint16_t next_port_;

  std::list<PendingPacket> pending_packets_;

  base::WeakPtrFactory<FakePacketSocketFactory> weak_factory_{this};
};

}  // namespace sharing

#endif  // CHROME_SERVICES_SHARING_WEBRTC_TEST_FAKE_PACKET_SOCKET_FACTORY_H_
