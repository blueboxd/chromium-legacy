// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/openscreen/platform/udp_socket.h"

#include <utility>

#include "base/bind.h"
#include "base/containers/span.h"
#include "net/base/address_family.h"
#include "net/base/ip_endpoint.h"

namespace openscreen {
namespace platform {

namespace {
constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("open_screen_message", R"(
        semantics {
          sender: "Open Screen"
          description:
            "Open Screen messages are used by the third_party Open Screen "
            "library, in accordance to the specification defined by the Open "
            "Screen protocol. The protocol is available publicly at: "
            "https://github.com/webscreens/openscreenprotocol"
          trigger:
            "Any message that needs to be sent or received by the Open Screen "
            "library."
          data:
            "Messages defined by the Open Screen Protocol specification."
          destination: OTHER
          destination_other:
            "The connection is made to an Open Screen endpoint on the LAN "
            "selected by the user, i.e. via a dialog."
        }
        policy {
          cookies_allowed: NO
          setting:
            "This request cannot be disabled, but it would not be sent if user "
            "does not connect to a Open Screen endpoint on the local network."
          policy_exception_justification: "Not implemented."
        })");

const auto kMutableTrafficAnnotation =
    net::CreateMutableNetworkTrafficAnnotationTag(
        kTrafficAnnotation.unique_id_hash_code);

const net::IPAddress ToChromeNetAddress(const IPAddress& address) {
  if (address.IsV4()) {
    std::array<uint8_t, IPAddress::kV4Size> bytes_v4;
    address.CopyToV4(bytes_v4.data());
    return net::IPAddress(bytes_v4.data(), bytes_v4.size());
  }

  std::array<uint8_t, IPAddress::kV6Size> bytes_v6;
  address.CopyToV6(bytes_v6.data());
  return net::IPAddress(bytes_v6.data(), bytes_v6.size());
}

const net::IPEndPoint ToChromeNetEndpoint(const IPEndpoint& endpoint) {
  return net::IPEndPoint(ToChromeNetAddress(endpoint.address), endpoint.port);
}

}  // namespace

// static
ErrorOr<UdpSocketUniquePtr> UdpSocket::Create(
    TaskRunner* task_runner,
    Client* client,
    const IPEndpoint& local_endpoint) {
  // TODO(jophba): implement create method.
  NOTIMPLEMENTED();
  return Error::None();
}

ChromeUdpSocket::ChromeUdpSocket(std::unique_ptr<network::UDPSocket> udp_socket,
                                 TaskRunner* task_runner,
                                 Client* client,
                                 const IPEndpoint& local_endpoint)
    : UdpSocket(task_runner, client),
      local_endpoint_(ToChromeNetEndpoint(local_endpoint)),
      udp_socket_(std::move(udp_socket)) {}
ChromeUdpSocket::~ChromeUdpSocket() = default;

bool ChromeUdpSocket::IsIPv4() const {
  return local_endpoint_.GetFamily() == net::AddressFamily::ADDRESS_FAMILY_IPV4;
}

bool ChromeUdpSocket::IsIPv6() const {
  return local_endpoint_.GetFamily() == net::AddressFamily::ADDRESS_FAMILY_IPV6;
}

Error ChromeUdpSocket::Bind() {
  udp_socket_->Bind(local_endpoint_, nullptr,
                    base::BindOnce(&ChromeUdpSocket::BindCallback,
                                   weak_ptr_factory_.GetWeakPtr()));

  // Errors are reported using the callback message, so no error to return here.
  return Error::None();
}

// TODO(jophba): how does the network service handle multiple interfaces?
// mojom::UDPSocket doesn't have a concept of network interface indices, so
// the ifindex argument is ignored here.
Error ChromeUdpSocket::SetMulticastOutboundInterface(
    NetworkInterfaceIndex ifindex) {
  return Error::Code::kNotImplemented;
}

// mojom::UDPSocket doesn't have a concept of network interface indices, so
// the ifindex argument is ignored here.
Error ChromeUdpSocket::JoinMulticastGroup(const IPAddress& address,
                                          NetworkInterfaceIndex ifindex) {
  const auto join_address = ToChromeNetAddress(address);
  udp_socket_->JoinGroup(join_address,
                         base::BindOnce(&ChromeUdpSocket::JoinGroupCallback,
                                        weak_ptr_factory_.GetWeakPtr()));

  // Errors are reported using the callback message, so no error to return here.
  return Error::None();
}

// mojom::UDPSocket has the receiver set before construction, so receiving
// messages in this fashion doesn't actually make sense.
ErrorOr<UdpPacket> ChromeUdpSocket::ReceiveMessage() {
  return Error::Code::kNotImplemented;
}

Error ChromeUdpSocket::SendMessage(const void* data,
                                   size_t length,
                                   const IPEndpoint& dest) {
  const auto send_to_address = ToChromeNetEndpoint(dest);
  const base::span<const uint8_t> data_span(
      reinterpret_cast<const uint8_t*>(data), length);

  udp_socket_->SendTo(send_to_address, data_span, kMutableTrafficAnnotation,
                      base::BindOnce(&ChromeUdpSocket::SendToCallback,
                                     weak_ptr_factory_.GetWeakPtr()));

  // Errors are reported using the callback message, so no error to return here.
  return Error::None();
}

// mojom::UDPSocket doesn't have a concept of DSCP, so this is a noop.
Error ChromeUdpSocket::SetDscp(DscpMode state) {
  return Error::Code::kNotImplemented;
}

// TODO(jophba): change UDPSocket API to use callbacks, so clients don't have
// to randomly check GetLastError().
int32_t ChromeUdpSocket::GetLastError() {
  return last_error_;
}

void ChromeUdpSocket::BindCallback(
    int32_t result,
    const base::Optional<net::IPEndPoint>& address) {
  if (result != 0) {
    last_error_ = result;
  } else {
    is_bound_ = true;
  }
}

void ChromeUdpSocket::JoinGroupCallback(int32_t result) {
  if (result != 0) {
    last_error_ = result;
  }
}

void ChromeUdpSocket::SendToCallback(int32_t result) {
  if (result != 0) {
    last_error_ = result;
  }
}

}  // namespace platform
}  // namespace openscreen
