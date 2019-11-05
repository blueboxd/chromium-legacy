// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/quic_transport.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/webtransport/quic_transport_connector.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

QuicTransport* QuicTransport::Create(ScriptState* script_state,
                                     const String& url) {
  auto* transport =
      MakeGarbageCollected<QuicTransport>(PassKey(), script_state, url);
  transport->Init();
  return transport;
}

QuicTransport::QuicTransport(PassKey,
                             ScriptState* script_state,
                             const String& url)
    : script_state_(script_state),
      url_(url),
      handshake_client_receiver_(this) {}

void QuicTransport::OnConnectionEstablished(
    mojo::PendingRemote<network::mojom::blink::QuicTransport>,
    mojo::PendingReceiver<network::mojom::blink::QuicTransportClient>) {
  handshake_client_receiver_.reset();
}

void QuicTransport::OnHandshakeFailed() {
  handshake_client_receiver_.reset();
}

void QuicTransport::Trace(Visitor* visitor) {
  visitor->Trace(script_state_);
  ScriptWrappable::Trace(visitor);
}

void QuicTransport::Init() {
  mojo::Remote<mojom::blink::QuicTransportConnector> connector;
  ExecutionContext* execution_context = ExecutionContext::From(script_state_);

  DCHECK(execution_context->GetInterfaceProvider());
  execution_context->GetInterfaceProvider()->GetInterface(
      connector.BindNewPipeAndPassReceiver(
          execution_context->GetTaskRunner(TaskType::kNetworking)));

  connector->Connect(
      url_, handshake_client_receiver_.BindNewPipeAndPassRemote(
                execution_context->GetTaskRunner(TaskType::kNetworking)));
  // TODO(yhirano): Attach a disconnect handler for
  // |handshake_client_receiver_|.
}

void QuicTransport::Dispose() {
  handshake_client_receiver_.reset();
}

}  // namespace blink
