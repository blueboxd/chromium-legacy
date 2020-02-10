// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_CAST_STREAMING_RECEIVER_UDP_SOCKET_H_
#define FUCHSIA_CAST_STREAMING_RECEIVER_UDP_SOCKET_H_

#include <memory>

#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/socket/udp_socket.h"
#include "third_party/openscreen/src/platform/api/udp_socket.h"

class ReceiverUdpSocket : public openscreen::UdpSocket {
 public:
  ReceiverUdpSocket(Client* client,
                    const openscreen::IPEndpoint& local_endpoint);
  ~ReceiverUdpSocket() final;

  ReceiverUdpSocket& operator=(const ReceiverUdpSocket&) = delete;
  ReceiverUdpSocket& operator=(ReceiverUdpSocket&&) = delete;

  // openscreen::UdpSocket implementation.
  bool IsIPv4() const final;
  bool IsIPv6() const final;
  openscreen::IPEndpoint GetLocalEndpoint() const final;
  void Bind() final;
  void SetMulticastOutboundInterface(
      openscreen::NetworkInterfaceIndex ifindex) final;
  void JoinMulticastGroup(const openscreen::IPAddress& address,
                          openscreen::NetworkInterfaceIndex ifindex) final;
  void SendMessage(const void* data,
                   size_t length,
                   const openscreen::IPEndpoint& dest) final;
  void SetDscp(openscreen::UdpSocket::DscpMode state) final;

 private:
  void SendErrorToClient(openscreen::Error::Code openscreen_error,
                         int net_error);
  void DoRead();
  bool HandleReadResult(int result);
  void OnRecvFromCompleted(int result);
  void OnSendToCompleted(int result);

  Client* const client_;

  // The local endpoint can change as a result of bind calls.
  openscreen::IPEndpoint local_endpoint_;
  net::UDPSocket udp_socket_;

  scoped_refptr<net::IOBuffer> read_buffer_;
  net::IPEndPoint from_address_;
  bool send_pending_ = false;
};

#endif  // FUCHSIA_CAST_STREAMING_RECEIVER_UDP_SOCKET_H_
