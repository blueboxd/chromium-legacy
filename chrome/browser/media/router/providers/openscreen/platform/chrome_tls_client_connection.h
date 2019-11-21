// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_PLATFORM_CHROME_TLS_CLIENT_CONNECTION_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_PLATFORM_CHROME_TLS_CLIENT_CONNECTION_H_

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/media/router/providers/openscreen/platform/network_data_pump.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/io_buffer.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/network/public/mojom/tls_socket.mojom.h"
#include "third_party/openscreen/src/platform/api/task_runner.h"
#include "third_party/openscreen/src/platform/api/tls_connection.h"
#include "third_party/openscreen/src/platform/base/ip_address.h"

namespace media_router {

// Average packet size is difficult to determine, but the MTU for ethernet
// is 1500 bytes so it's a good place to start. If we get data larger than this,
// it will have to spread over multiple calls.
constexpr size_t kMaxTlsPacketBufferSize = 1500;

// This is ref-counted.
class VectorIOBuffer : public net::IOBuffer {
 public:
  VectorIOBuffer();
  std::vector<uint8_t> TakeResult();
  inline size_t size() const { return buffer_.size(); }

 private:
  ~VectorIOBuffer() final;
  std::vector<uint8_t> buffer_;
};

class ChromeTlsClientConnection : public openscreen::platform::TlsConnection {
 public:
  ChromeTlsClientConnection(
      openscreen::platform::TaskRunner* task_runner,
      openscreen::IPEndpoint local_address,
      openscreen::IPEndpoint remote_address,
      std::unique_ptr<NetworkDataPump> data_pump,
      mojo::Remote<network::mojom::TCPConnectedSocket> tcp_socket,
      mojo::Remote<network::mojom::TLSClientSocket> tls_socket);

  ~ChromeTlsClientConnection() final;

  // TlsConnection overrides.
  void SetClient(Client* client) final;
  void Write(const void* data, size_t len) final;
  openscreen::IPEndpoint GetLocalEndpoint() const final;
  openscreen::IPEndpoint GetRemoteEndpoint() const final;

 private:
  // Data pipe completion callbacks.
  void OnReadComplete(int32_t size_or_error);
  void OnWriteComplete(int32_t size_or_error);
  void RequestRead();

  Client* client_ = nullptr;
  openscreen::platform::TaskRunner* task_runner_ = nullptr;
  openscreen::IPEndpoint local_address_;
  openscreen::IPEndpoint remote_address_;

  std::unique_ptr<NetworkDataPump> data_pump_;

  scoped_refptr<VectorIOBuffer> read_buffer_;
  scoped_refptr<net::GrowableIOBuffer> write_buffer_;
  bool write_blocked_ = false;

  mojo::Remote<network::mojom::TCPConnectedSocket> tcp_socket_;
  mojo::Remote<network::mojom::TLSClientSocket> tls_socket_;

  base::WeakPtrFactory<ChromeTlsClientConnection> weak_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_OPENSCREEN_PLATFORM_CHROME_TLS_CLIENT_CONNECTION_H_
