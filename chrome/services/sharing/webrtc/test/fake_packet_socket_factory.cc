// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/webrtc/test/fake_packet_socket_factory.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/numerics/math_constants.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "net/base/io_buffer.h"
#include "third_party/webrtc/media/base/rtp_utils.h"
#include "third_party/webrtc/rtc_base/async_packet_socket.h"
#include "third_party/webrtc/rtc_base/socket.h"
#include "third_party/webrtc/rtc_base/time_utils.h"

namespace sharing {

namespace {

const int kPortRangeStart = 1024;
const int kPortRangeEnd = 65535;

class FakeUdpSocket : public rtc::AsyncPacketSocket {
 public:
  FakeUdpSocket(FakePacketSocketFactory* factory,
                const rtc::SocketAddress& local_address)
      : factory_(factory), local_address_(local_address), state_(STATE_BOUND) {}
  FakeUdpSocket(const FakeUdpSocket&) = delete;
  FakeUdpSocket& operator=(const FakeUdpSocket&) = delete;
  ~FakeUdpSocket() override {
    factory_->OnSocketDestroyed(local_address_.port());
  }

  void ReceivePacket(const rtc::SocketAddress& from,
                     const rtc::SocketAddress& to,
                     const scoped_refptr<net::IOBuffer>& data,
                     int data_size) {
    SignalReadPacket(this, data->data(), data_size, from, rtc::TimeMicros());
  }

  // rtc::AsyncPacketSocket interface.
  rtc::SocketAddress GetLocalAddress() const override { return local_address_; }

  rtc::SocketAddress GetRemoteAddress() const override {
    NOTREACHED();
    return rtc::SocketAddress();
  }

  int Send(const void* data,
           size_t data_size,
           const rtc::PacketOptions& options) override {
    NOTREACHED();
    return EINVAL;
  }

  int SendTo(const void* data,
             size_t data_size,
             const rtc::SocketAddress& address,
             const rtc::PacketOptions& options) override {
    scoped_refptr<net::IOBuffer> buffer =
        base::MakeRefCounted<net::IOBuffer>(data_size);
    memcpy(buffer->data(), data, data_size);
    base::TimeTicks now = base::TimeTicks::Now();
    cricket::ApplyPacketOptions(reinterpret_cast<uint8_t*>(buffer->data()),
                                data_size, options.packet_time_params,
                                (now - base::TimeTicks()).InMicroseconds());
    SignalSentPacket(this,
                     rtc::SentPacket(options.packet_id, rtc::TimeMillis()));
    factory_->ReceivePacket(local_address_, address, buffer, data_size);
    return data_size;
  }

  int Close() override {
    state_ = STATE_CLOSED;
    return 0;
  }

  State GetState() const override { return state_; }

  int GetOption(rtc::Socket::Option option, int* value) override {
    NOTIMPLEMENTED();
    return -1;
  }

  int SetOption(rtc::Socket::Option option, int value) override {
    // All options are currently ignored.
    return 0;
  }

  int GetError() const override { return 0; }

  void SetError(int error) override { NOTREACHED(); }

 private:
  FakePacketSocketFactory* factory_;
  rtc::SocketAddress local_address_;
  State state_;
};

}  // namespace

FakePacketSocketFactory::PendingPacket::PendingPacket() : data_size(0) {}

FakePacketSocketFactory::PendingPacket::PendingPacket(
    const rtc::SocketAddress& from,
    const rtc::SocketAddress& to,
    const scoped_refptr<net::IOBuffer>& data,
    int data_size)
    : from(from), to(to), data(data), data_size(data_size) {}

FakePacketSocketFactory::PendingPacket::PendingPacket(
    const PendingPacket& other) = default;

FakePacketSocketFactory::PendingPacket::~PendingPacket() = default;

FakePacketSocketFactory::FakePacketSocketFactory(const rtc::IPAddress& address)
    : address_(address), next_port_(kPortRangeStart) {}

FakePacketSocketFactory::~FakePacketSocketFactory() {
  CHECK(udp_sockets_.empty());
}

void FakePacketSocketFactory::OnSocketDestroyed(int port) {
  udp_sockets_.erase(port);
}

rtc::AsyncPacketSocket* FakePacketSocketFactory::CreateUdpSocket(
    const rtc::SocketAddress& local_address,
    uint16_t min_port,
    uint16_t max_port) {
  int port = -1;
  uint16_t start = min_port > 0 ? min_port : next_port_;
  uint16_t end = max_port > 0 ? max_port : kPortRangeEnd;
  DCHECK(start <= end);
  DCHECK(local_address.ipaddr() == address_);

  for (uint16_t i = start; i <= end; ++i) {
    if (udp_sockets_.find(i) == udp_sockets_.end()) {
      port = i;
      break;
    }
  }

  if (port < 0)
    return nullptr;

  FakeUdpSocket* socket =
      new FakeUdpSocket(this, rtc::SocketAddress(local_address.ipaddr(), port));

  udp_sockets_[port] =
      base::Bind(&FakeUdpSocket::ReceivePacket, base::Unretained(socket));

  return socket;
}

rtc::AsyncPacketSocket* FakePacketSocketFactory::CreateServerTcpSocket(
    const rtc::SocketAddress& local_address,
    uint16_t min_port,
    uint16_t max_port,
    int opts) {
  return nullptr;
}

rtc::AsyncPacketSocket* FakePacketSocketFactory::CreateClientTcpSocket(
    const rtc::SocketAddress& local_address,
    const rtc::SocketAddress& remote_address,
    const rtc::ProxyInfo& proxy_info,
    const std::string& user_agent,
    const rtc::PacketSocketTcpOptions& opts) {
  return nullptr;
}

rtc::AsyncResolverInterface* FakePacketSocketFactory::CreateAsyncResolver() {
  return nullptr;
}

void FakePacketSocketFactory::ReceivePacket(
    const rtc::SocketAddress& from,
    const rtc::SocketAddress& to,
    const scoped_refptr<net::IOBuffer>& data,
    int data_size) {
  DCHECK(to.ipaddr() == address_);

  PendingPacket packet(from, to, data, data_size);
  pending_packets_.push_back(packet);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FakePacketSocketFactory::DoReceivePacket,
                                weak_factory_.GetWeakPtr()));
}

void FakePacketSocketFactory::DoReceivePacket() {
  PendingPacket packet = pending_packets_.front();
  pending_packets_.pop_front();

  auto iter = udp_sockets_.find(packet.to.port());
  if (iter == udp_sockets_.end()) {
    // Invalid port number.
    return;
  }

  iter->second.Run(packet.from, packet.to, packet.data, packet.data_size);
}

}  // namespace sharing
