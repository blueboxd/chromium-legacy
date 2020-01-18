// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_PLATFORM_CHROME_TLS_CLIENT_CONNECTION_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_PLATFORM_CHROME_TLS_CLIENT_CONNECTION_H_

#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/network/public/mojom/tls_socket.mojom.h"
#include "third_party/openscreen/src/platform/api/task_runner.h"
#include "third_party/openscreen/src/platform/api/tls_connection.h"
#include "third_party/openscreen/src/platform/base/error.h"
#include "third_party/openscreen/src/platform/base/ip_address.h"

namespace media_router {

class ChromeTlsClientConnection : public openscreen::TlsConnection {
 public:
  ChromeTlsClientConnection(
      openscreen::TaskRunner* task_runner,
      openscreen::IPEndpoint local_address,
      openscreen::IPEndpoint remote_address,
      mojo::ScopedDataPipeConsumerHandle receive_stream,
      mojo::ScopedDataPipeProducerHandle send_stream,
      mojo::Remote<network::mojom::TCPConnectedSocket> tcp_socket,
      mojo::Remote<network::mojom::TLSClientSocket> tls_socket);

  ~ChromeTlsClientConnection() final;

  // TlsConnection overrides.
  void SetClient(Client* client) final;
  bool Send(const void* data, size_t len) final;
  openscreen::IPEndpoint GetLocalEndpoint() const final;
  openscreen::IPEndpoint GetRemoteEndpoint() const final;

  // The maximum size of the vector in any single Client::OnRead() callback.
  static constexpr uint32_t kMaxBytesPerRead = 64 << 10;  // 64 KB.

 private:
  // Invoked by |receive_stream_watcher_| when the |receive_stream_|'s status
  // has changed. Calls Client::OnRead() if data has become available.
  void ReceiveMore(MojoResult result, const mojo::HandleSignalsState& state);

  // Classifies MojoResult codes into one of three categories: kOk, kAgain for
  // transient errors, or |error_code_if_fatal| for fatal errors. If the result
  // is a fatal error, this also invokes Client::OnError().
  openscreen::Error::Code ProcessMojoResult(
      MojoResult result,
      openscreen::Error::Code error_code_if_fatal);

  openscreen::TaskRunner* const task_runner_ = nullptr;
  const openscreen::IPEndpoint local_address_;
  const openscreen::IPEndpoint remote_address_;
  const mojo::ScopedDataPipeConsumerHandle receive_stream_;
  const mojo::ScopedDataPipeProducerHandle send_stream_;
  const mojo::Remote<network::mojom::TCPConnectedSocket> tcp_socket_;
  const mojo::Remote<network::mojom::TLSClientSocket> tls_socket_;

  mojo::SimpleWatcher receive_stream_watcher_;

  Client* client_ = nullptr;
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_PLATFORM_CHROME_TLS_CLIENT_CONNECTION_H_
