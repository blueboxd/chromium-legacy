// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/cast_streaming/receiver_udp_socket.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "components/openscreen_platform/network_util.h"
#include "net/base/net_errors.h"

namespace openscreen {

// static
ErrorOr<std::unique_ptr<UdpSocket>> UdpSocket::Create(
    TaskRunner* task_runner,
    Client* client,
    const IPEndpoint& local_endpoint) {
  return ErrorOr<std::unique_ptr<UdpSocket>>(
      std::make_unique<ReceiverUdpSocket>(client, local_endpoint));
}

}  // namespace openscreen

namespace {

// UDP packaet size is limited to 64k.
constexpr size_t kBufferSize = 65536;

}  // namespace

ReceiverUdpSocket::ReceiverUdpSocket(
    openscreen::UdpSocket::Client* client,
    const openscreen::IPEndpoint& local_endpoint)
    : client_(client),
      local_endpoint_(local_endpoint),
      udp_socket_(net::DatagramSocket::DEFAULT_BIND,
                  nullptr /* net_log */,
                  net::NetLogSource()),
      read_buffer_(base::MakeRefCounted<net::IOBuffer>(kBufferSize)) {
  DCHECK(client_);
}

ReceiverUdpSocket::~ReceiverUdpSocket() = default;

bool ReceiverUdpSocket::IsIPv4() const {
  return local_endpoint_.address.IsV4();
}

bool ReceiverUdpSocket::IsIPv6() const {
  return local_endpoint_.address.IsV6();
}

openscreen::IPEndpoint ReceiverUdpSocket::GetLocalEndpoint() const {
  return local_endpoint_;
}

void ReceiverUdpSocket::Bind() {
  net::IPEndPoint endpoint =
      openscreen_platform::ToNetEndPoint(local_endpoint_);
  int result = udp_socket_.Open(endpoint.GetFamily());
  if (result != net::OK) {
    SendErrorToClient(openscreen::Error::Code::kSocketBindFailure, result);
    return;
  }

  result = udp_socket_.Bind(endpoint);
  net::IPEndPoint local_endpoint;
  if (result == net::OK)
    result = udp_socket_.GetLocalAddress(&local_endpoint);

  if (result != net::OK) {
    SendErrorToClient(openscreen::Error::Code::kSocketBindFailure, result);
    return;
  }

  local_endpoint_ = openscreen_platform::ToOpenScreenEndPoint(local_endpoint);
  DoRead();
}

void ReceiverUdpSocket::SetMulticastOutboundInterface(
    openscreen::NetworkInterfaceIndex ifindex) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ReceiverUdpSocket::JoinMulticastGroup(
    const openscreen::IPAddress& address,
    openscreen::NetworkInterfaceIndex ifindex) {
  int result = udp_socket_.SetMulticastInterface(ifindex);
  if (result == net::OK)
    udp_socket_.JoinGroup(openscreen_platform::ToNetAddress(address));

  if (result != net::OK) {
    SendErrorToClient(openscreen::Error::Code::kSocketOptionSettingFailure,
                      result);
  }
}

void ReceiverUdpSocket::SendMessage(const void* data,
                                    size_t length,
                                    const openscreen::IPEndpoint& dest) {
  // Do not attempt to send another UDP packet while a SendTo() operation is
  // still pending.
  if (send_pending_)
    return;

  auto buffer = base::MakeRefCounted<net::IOBuffer>(length);
  memcpy(buffer->data(), data, length);

  int result = udp_socket_.SendTo(
      buffer.get(), length, openscreen_platform::ToNetEndPoint(dest),
      base::BindOnce(&ReceiverUdpSocket::OnSendToCompleted,
                     base::Unretained(this)));
  send_pending_ = true;

  if (result != net::ERR_IO_PENDING)
    OnSendToCompleted(result);
}

void ReceiverUdpSocket::SetDscp(openscreen::UdpSocket::DscpMode state) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void ReceiverUdpSocket::SendErrorToClient(
    openscreen::Error::Code openscreen_error,
    int net_error) {
  client_->OnError(
      this, openscreen::Error(openscreen_error, net::ErrorToString(net_error)));
}

void ReceiverUdpSocket::DoRead() {
  while (true) {
    int result = udp_socket_.RecvFrom(
        read_buffer_.get(), kBufferSize, &from_address_,
        base::BindOnce(&ReceiverUdpSocket::OnRecvFromCompleted,
                       base::Unretained(this)));
    if (result == net::ERR_IO_PENDING || !HandleReadResult(result))
      return;
  }
}

bool ReceiverUdpSocket::HandleReadResult(int result) {
  if (result < 0) {
    client_->OnRead(
        this, openscreen::Error(openscreen::Error::Code::kSocketReadFailure,
                                net::ErrorToString(result)));
    return false;
  }

  DCHECK_GT(result, 0);

  openscreen::UdpPacket packet(read_buffer_->data(),
                               read_buffer_->data() + result);
  packet.set_socket(this);
  packet.set_source(openscreen_platform::ToOpenScreenEndPoint(from_address_));
  client_->OnRead(this, std::move(packet));
  return true;
}

void ReceiverUdpSocket::OnRecvFromCompleted(int result) {
  if (HandleReadResult(result))
    DoRead();
}

void ReceiverUdpSocket::OnSendToCompleted(int result) {
  send_pending_ = false;
  if (result < 0) {
    client_->OnSendError(
        this, openscreen::Error(openscreen::Error::Code::kSocketSendFailure,
                                net::ErrorToString(result)));
    return;
  }
}
