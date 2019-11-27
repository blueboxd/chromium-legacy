// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/openscreen/platform/chrome_tls_connection_factory.h"

#include <utility>

#include "base/logging.h"
#include "chrome/browser/media/router/providers/openscreen/platform/chrome_tls_client_connection.h"
#include "chrome/browser/media/router/providers/openscreen/platform/mojo_data_pump.h"
#include "chrome/browser/media/router/providers/openscreen/platform/network_util.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/ssl/ssl_info.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/openscreen/src/platform/api/tls_connection.h"
#include "third_party/openscreen/src/platform/base/tls_connect_options.h"
#include "third_party/openscreen/src/platform/base/tls_credentials.h"
#include "third_party/openscreen/src/platform/base/tls_listen_options.h"

namespace openscreen {
namespace platform {
std::unique_ptr<TlsConnectionFactory> TlsConnectionFactory::CreateFactory(
    Client* client,
    TaskRunner* task_runner) {
  return std::make_unique<media_router::ChromeTlsConnectionFactory>(
      client, task_runner, nullptr /* network context */);
}
}  // namespace platform
}  // namespace openscreen

namespace media_router {

namespace {

using openscreen::IPEndpoint;
using openscreen::platform::TlsConnectOptions;
using openscreen::platform::TlsCredentials;
using openscreen::platform::TlsListenOptions;

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("open_screen_tls_message", R"(
        semantics {
          sender: "Open Screen"
          description:
            "Open Screen TLS messages are used by the third_party Open Screen "
            "library, primarily for the libcast CastTransport implementation."
          trigger:
            "Any TLS encrypted message that needs to be sent or received by "
            "the Open Screen library."
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

}  // namespace

ChromeTlsConnectionFactory::~ChromeTlsConnectionFactory() = default;

void ChromeTlsConnectionFactory::Connect(const IPEndpoint& remote_address,
                                         const TlsConnectOptions& options) {
  network::mojom::NetworkContext* network_context = network_context_;
  if (!network_context) {
    auto* const manager = SystemNetworkContextManager::GetInstance();
    network_context = manager ? manager->GetContext() : nullptr;
  }

  if (!network_context) {
    client_->OnError(this, openscreen::Error::Code::kItemNotFound);
    return;
  }

  TcpConnectRequest request(options, remote_address,
                            mojo::Remote<network::mojom::TCPConnectedSocket>{});

  const net::AddressList address_list(
      media_router::network_util::ToChromeNetEndpoint(remote_address));

  network_context->CreateTCPConnectedSocket(
      base::nullopt /* local_addr */, address_list,
      nullptr /* tcp_connected_socket_options */,
      net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation),
      request.tcp_socket.BindNewPipeAndPassReceiver(),
      mojo::NullRemote(), /* observer */
      base::BindOnce(&ChromeTlsConnectionFactory::OnTcpConnect,
                     weak_factory_.GetWeakPtr(), std::move(request)));
}

void ChromeTlsConnectionFactory::SetListenCredentials(
    const TlsCredentials& credentials) {
  NOTIMPLEMENTED();
}

void ChromeTlsConnectionFactory::Listen(const IPEndpoint& local_address,
                                        const TlsListenOptions& options) {
  NOTIMPLEMENTED();
}

ChromeTlsConnectionFactory::ChromeTlsConnectionFactory(
    openscreen::platform::TlsConnectionFactory::Client* client,
    openscreen::platform::TaskRunner* task_runner,
    network::mojom::NetworkContext* network_context)
    : client_(client),
      task_runner_(task_runner),
      network_context_(network_context) {}

ChromeTlsConnectionFactory::TcpConnectRequest::TcpConnectRequest(
    openscreen::platform::TlsConnectOptions options_in,
    openscreen::IPEndpoint remote_address_in,
    mojo::Remote<network::mojom::TCPConnectedSocket> tcp_socket_in)
    : options(std::move(options_in)),
      remote_address(std::move(remote_address_in)),
      tcp_socket(std::move(tcp_socket_in)) {}
ChromeTlsConnectionFactory::TcpConnectRequest::TcpConnectRequest(
    TcpConnectRequest&&) = default;
ChromeTlsConnectionFactory::TcpConnectRequest&
ChromeTlsConnectionFactory::TcpConnectRequest::operator=(TcpConnectRequest&&) =
    default;

ChromeTlsConnectionFactory::TcpConnectRequest::~TcpConnectRequest() = default;

ChromeTlsConnectionFactory::TlsUpgradeRequest::TlsUpgradeRequest(
    openscreen::IPEndpoint local_address_in,
    openscreen::IPEndpoint remote_address_in,
    mojo::Remote<network::mojom::TCPConnectedSocket> tcp_socket_in,
    mojo::Remote<network::mojom::TLSClientSocket> tls_socket_in)
    : local_address(std::move(local_address_in)),
      remote_address(std::move(remote_address_in)),
      tcp_socket(std::move(tcp_socket_in)),
      tls_socket(std::move(tls_socket_in)) {}
ChromeTlsConnectionFactory::TlsUpgradeRequest::TlsUpgradeRequest(
    TlsUpgradeRequest&&) = default;
ChromeTlsConnectionFactory::TlsUpgradeRequest&
ChromeTlsConnectionFactory::TlsUpgradeRequest::operator=(TlsUpgradeRequest&&) =
    default;
ChromeTlsConnectionFactory::TlsUpgradeRequest::~TlsUpgradeRequest() = default;

void ChromeTlsConnectionFactory::OnTcpConnect(
    TcpConnectRequest request,
    int32_t net_result,
    const base::Optional<net::IPEndPoint>& local_address,
    const base::Optional<net::IPEndPoint>& remote_address,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream) {
  // We only care about net_result, since local_address doesn't matter,
  // remote_address should be 1 out of 1 addresses provided in the connect
  // call, and the streams must be closed before upgrading is allowed.
  if (net_result != net::OK) {
    client_->OnConnectionFailed(this, request.remote_address);
    return;
  }

  net::HostPortPair host_port_pair = net::HostPortPair::FromIPEndPoint(
      network_util::ToChromeNetEndpoint(request.remote_address));
  network::mojom::TLSClientSocketOptionsPtr options =
      network::mojom::TLSClientSocketOptions::New();
  options->unsafely_skip_cert_verification =
      request.options.unsafely_skip_certificate_validation;

  openscreen::IPEndpoint local_endpoint{};
  if (local_address) {
    local_endpoint = network_util::ToOpenScreenEndpoint(local_address.value());
  }

  TlsUpgradeRequest upgrade_request(
      std::move(request.remote_address), std::move(local_endpoint),
      std::move(request.tcp_socket),
      mojo::Remote<network::mojom::TLSClientSocket>{});

  upgrade_request.tcp_socket->UpgradeToTLS(
      host_port_pair, std::move(options),
      net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation),
      upgrade_request.tls_socket.BindNewPipeAndPassReceiver(),
      mojo::NullRemote() /* observer */,
      base::BindOnce(&ChromeTlsConnectionFactory::OnTlsUpgrade,
                     weak_factory_.GetWeakPtr(), std::move(upgrade_request)));
}

void ChromeTlsConnectionFactory::OnTlsUpgrade(
    TlsUpgradeRequest request,
    int32_t net_result,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream,
    const base::Optional<net::SSLInfo>& ssl_info) {
  if (net_result != net::OK) {
    client_->OnConnectionFailed(this, request.remote_address);
    return;
  }

  auto data_pump = std::make_unique<MojoDataPump>(std::move(receive_stream),
                                                  std::move(send_stream));

  auto tls_connection = std::make_unique<ChromeTlsClientConnection>(
      task_runner_, request.local_address, request.remote_address,
      std::move(data_pump), std::move(request.tcp_socket),
      std::move(request.tls_socket));

  // TODO(crbug.com/1017903): populate X509 certificate field when it is
  // migrated to a CRYPTO_BUFFER. For motivation, see:
  // https://boringssl.googlesource.com/boringssl/+/HEAD/PORTING.md
  std::vector<uint8_t> der_x509_certificate;
  client_->OnConnected(this, der_x509_certificate, std::move(tls_connection));
}

}  // namespace media_router
