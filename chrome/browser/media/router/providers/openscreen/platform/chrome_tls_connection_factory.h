// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_PLATFORM_CHROME_TLS_CONNECTION_FACTORY_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_PLATFORM_CHROME_TLS_CONNECTION_FACTORY_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/network/public/mojom/tls_socket.mojom.h"
#include "third_party/openscreen/src/platform/api/tls_connection_factory.h"
#include "third_party/openscreen/src/platform/base/tls_connect_options.h"

namespace net {
class IPEndPoint;
}

namespace openscreen {
struct IPEndpoint;

namespace platform {
struct TlsCredentials;
struct TlsListenOptions;
}  // namespace platform

}  // namespace openscreen

namespace media_router {

class ChromeTlsConnectionFactory
    : public openscreen::platform::TlsConnectionFactory {
 public:
  // If provided, the network context is stored and dereferenced when attempting
  // to connect. If not provided, the network context is dynamically looked up.
  ChromeTlsConnectionFactory(
      openscreen::platform::TlsConnectionFactory::Client* client,
      openscreen::platform::TaskRunner* task_runner,
      network::mojom::NetworkContext* network_context);

  ~ChromeTlsConnectionFactory() final;

  // TlsConnectionFactory overrides
  void Connect(const openscreen::IPEndpoint& remote_address,
               const openscreen::platform::TlsConnectOptions& options) final;

  // Since Chrome doesn't implement TLS server sockets, these methods are not
  // implemented.
  void SetListenCredentials(
      const openscreen::platform::TlsCredentials& credentials) final;
  void Listen(const openscreen::IPEndpoint& local_address,
              const openscreen::platform::TlsListenOptions& options) final;

 private:
  // Note on TcpConnectRequest and TlsUpgradeRequest:
  // These classes are used to manage connection state for creating TCP.
  // connections and upgrading them to TLS. They are movable, but not copyable,
  // due to unique ownership of the mojo::Remotes, and passed into the TCP/TLS
  // callbacks (OnTcpConnect and OnTlsUpgrade) using currying.
  struct TcpConnectRequest {
    TcpConnectRequest(
        openscreen::platform::TlsConnectOptions options_in,
        openscreen::IPEndpoint remote_address_in,
        mojo::Remote<network::mojom::TCPConnectedSocket> tcp_socket_in);
    TcpConnectRequest(const TcpConnectRequest&) = delete;
    TcpConnectRequest(TcpConnectRequest&&);
    TcpConnectRequest& operator=(const TcpConnectRequest&) = delete;
    TcpConnectRequest& operator=(TcpConnectRequest&&);
    ~TcpConnectRequest();

    openscreen::platform::TlsConnectOptions options;
    openscreen::IPEndpoint remote_address;
    mojo::Remote<network::mojom::TCPConnectedSocket> tcp_socket;
  };

  struct TlsUpgradeRequest {
    TlsUpgradeRequest(
        openscreen::IPEndpoint local_address_in,
        openscreen::IPEndpoint remote_address_in,
        mojo::Remote<network::mojom::TCPConnectedSocket> tcp_socket_in,
        mojo::Remote<network::mojom::TLSClientSocket> tls_socket_in);
    TlsUpgradeRequest(const TlsUpgradeRequest&) = delete;
    TlsUpgradeRequest(TlsUpgradeRequest&&);
    TlsUpgradeRequest& operator=(const TlsUpgradeRequest&) = delete;
    TlsUpgradeRequest& operator=(TlsUpgradeRequest&&);
    ~TlsUpgradeRequest();

    openscreen::IPEndpoint local_address;
    openscreen::IPEndpoint remote_address;
    mojo::Remote<network::mojom::TCPConnectedSocket> tcp_socket;
    mojo::Remote<network::mojom::TLSClientSocket> tls_socket;
  };

  void OnTcpConnect(TcpConnectRequest request,
                    int32_t net_result,
                    const base::Optional<net::IPEndPoint>& local_address,
                    const base::Optional<net::IPEndPoint>& remote_address,
                    mojo::ScopedDataPipeConsumerHandle receive_stream,
                    mojo::ScopedDataPipeProducerHandle send_stream);

  void OnTlsUpgrade(TlsUpgradeRequest request,
                    int32_t net_result,
                    mojo::ScopedDataPipeConsumerHandle receive_stream,
                    mojo::ScopedDataPipeProducerHandle send_stream,
                    const base::Optional<net::SSLInfo>& ssl_info);

  openscreen::platform::TlsConnectionFactory::Client* client_;
  openscreen::platform::TaskRunner* const task_runner_;
  network::mojom::NetworkContext* const network_context_;
  base::WeakPtrFactory<ChromeTlsConnectionFactory> weak_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_PLATFORM_CHROME_TLS_CONNECTION_FACTORY_H_
