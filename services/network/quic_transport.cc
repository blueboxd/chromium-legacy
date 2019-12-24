// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/quic_transport.h"

#include "net/base/io_buffer.h"
#include "net/quic/platform/impl/quic_mem_slice_impl.h"
#include "net/third_party/quiche/src/quic/core/quic_session.h"
#include "net/third_party/quiche/src/quic/core/quic_types.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_mem_slice.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_mem_slice_span.h"
#include "services/network/network_context.h"
#include "services/network/public/mojom/quic_transport.mojom.h"

namespace network {

QuicTransport::QuicTransport(
    const GURL& url,
    const url::Origin& origin,
    const net::NetworkIsolationKey& key,
    NetworkContext* context,
    mojo::PendingRemote<mojom::QuicTransportHandshakeClient> handshake_client)
    : transport_(std::make_unique<net::QuicTransportClient>(
          url,
          origin,
          this,
          key,
          context->url_request_context())),
      context_(context),
      receiver_(this),
      handshake_client_(std::move(handshake_client)) {
  handshake_client_.set_disconnect_handler(
      base::BindOnce(&QuicTransport::Dispose, base::Unretained(this)));

  transport_->Connect();
}

QuicTransport::~QuicTransport() = default;

void QuicTransport::SendDatagram(base::span<const uint8_t> data,
                                 base::OnceCallback<void(bool)> callback) {
  auto buffer = base::MakeRefCounted<net::IOBuffer>(data.size());
  memcpy(buffer->data(), data.data(), data.size());
  quic::QuicMemSlice slice(
      quic::QuicMemSliceImpl(std::move(buffer), data.size()));
  const quic::MessageResult result =
      transport_->session()->SendMessage(quic::QuicMemSliceSpan(&slice));
  std::move(callback).Run(result.status == quic::MESSAGE_STATUS_SUCCESS);
}

void QuicTransport::OnConnected() {
  DCHECK(handshake_client_);

  handshake_client_->OnConnectionEstablished(
      receiver_.BindNewPipeAndPassRemote(),
      client_.BindNewPipeAndPassReceiver());

  handshake_client_.reset();
  client_.set_disconnect_handler(
      base::BindOnce(&QuicTransport::Dispose, base::Unretained(this)));
}

void QuicTransport::OnConnectionFailed() {
  DCHECK(handshake_client_);

  handshake_client_->OnHandshakeFailed();

  Dispose();
  // |this| is deleted.
}

void QuicTransport::OnClosed() {
  DCHECK(!handshake_client_);

  Dispose();
  // |this| is deleted.
}

void QuicTransport::OnError() {
  DCHECK(!handshake_client_);

  Dispose();
  // |this| is deleted.
}

void QuicTransport::OnIncomingBidirectionalStreamAvailable() {}

void QuicTransport::OnIncomingUnidirectionalStreamAvailable() {}

void QuicTransport::Dispose() {
  context_->Remove(this);
  // |this| is deleted.
}

}  // namespace network
