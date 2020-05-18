// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webtransport/quic_transport_connector_impl.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/quic/quic_transport_client.h"

namespace content {

namespace {

using network::mojom::QuicTransportHandshakeClient;

class InterceptingHandshakeClient final : public QuicTransportHandshakeClient {
 public:
  InterceptingHandshakeClient(
      base::WeakPtr<RenderFrameHostImpl> frame,
      const GURL& url,
      mojo::PendingRemote<QuicTransportHandshakeClient> remote)
      : frame_(std::move(frame)), url_(url), remote_(std::move(remote)) {}
  ~InterceptingHandshakeClient() override = default;

  // QuicTransportHandshakeClient implementation:
  void OnConnectionEstablished(
      mojo::PendingRemote<network::mojom::QuicTransport> transport,
      mojo::PendingReceiver<network::mojom::QuicTransportClient> client)
      override {
    remote_->OnConnectionEstablished(std::move(transport), std::move(client));
  }
  void OnHandshakeFailed(
      const base::Optional<net::QuicTransportError>& error) override {
    // Here we pass null because it is dangerous to pass the error details
    // to the initiator renderer.
    remote_->OnHandshakeFailed(base::nullopt);

    if (RenderFrameHostImpl* frame = frame_.get()) {
      devtools_instrumentation::OnQuicTransportHandshakeFailed(frame, url_,
                                                               error);
    }
  }

 private:
  const base::WeakPtr<RenderFrameHostImpl> frame_;
  const GURL url_;
  mojo::Remote<QuicTransportHandshakeClient> remote_;
};

}  // namespace

QuicTransportConnectorImpl::QuicTransportConnectorImpl(
    int process_id,
    base::WeakPtr<RenderFrameHostImpl> frame,
    const url::Origin& origin,
    const net::NetworkIsolationKey& network_isolation_key)
    : process_id_(process_id),
      frame_(std::move(frame)),
      origin_(origin),
      network_isolation_key_(network_isolation_key) {}

QuicTransportConnectorImpl::~QuicTransportConnectorImpl() = default;

void QuicTransportConnectorImpl::Connect(
    const GURL& url,
    mojo::PendingRemote<network::mojom::QuicTransportHandshakeClient>
        handshake_client) {
  RenderProcessHost* process = RenderProcessHost::FromID(process_id_);
  if (!process) {
    return;
  }

  mojo::PendingRemote<QuicTransportHandshakeClient> handshake_client_to_pass;
  // TODO(yhirano): Stop using MakeSelfOwnedReceiver here, because the
  // QuicTransport implementation in the network service won't notice that
  // the QuicTransportHandshakeClient is going away.
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<InterceptingHandshakeClient>(
          frame_, url, std::move(handshake_client)),
      handshake_client_to_pass.InitWithNewPipeAndPassReceiver());

  process->GetStoragePartition()->GetNetworkContext()->CreateQuicTransport(
      url, origin_, network_isolation_key_,
      std::move(handshake_client_to_pass));
}

}  // namespace content
