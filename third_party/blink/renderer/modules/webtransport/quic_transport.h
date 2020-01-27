// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_QUIC_TRANSPORT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_QUIC_TRANSPORT_H_

#include "base/containers/span.h"
#include "base/util/type_safety/pass_key.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/mojom/quic_transport.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class ReadableStream;
class ReadableStreamDefaultControllerWithScriptScope;
class ScriptPromiseResolver;
class ScriptState;
class WebTransportCloseInfo;
class WritableStream;

// https://wicg.github.io/web-transport/#quic-transport
class MODULES_EXPORT QuicTransport final
    : public ScriptWrappable,
      public ActiveScriptWrappable<QuicTransport>,
      public ContextLifecycleObserver,
      public network::mojom::blink::QuicTransportHandshakeClient,
      public network::mojom::blink::QuicTransportClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(QuicTransport, Dispose);
  USING_GARBAGE_COLLECTED_MIXIN(QuicTransport);

 public:
  using PassKey = util::PassKey<QuicTransport>;
  static QuicTransport* Create(ScriptState* script_state,
                               const String& url,
                               ExceptionState&);

  QuicTransport(PassKey, ScriptState*, const String& url);
  ~QuicTransport() override;

  // QuicTransport IDL implementation.
  WritableStream* sendDatagrams() { return outgoing_datagrams_; }
  ReadableStream* receiveDatagrams() { return received_datagrams_; }
  void close(const WebTransportCloseInfo*);
  ScriptPromise ready() { return ready_; }
  ScriptPromise closed() { return closed_; }

  // QuicTransportHandshakeClient implementation
  void OnConnectionEstablished(
      mojo::PendingRemote<network::mojom::blink::QuicTransport>,
      mojo::PendingReceiver<network::mojom::blink::QuicTransportClient>)
      override;
  void OnHandshakeFailed() override;

  // QuicTransportClient implementation
  void OnDatagramReceived(base::span<const uint8_t> data) override;
  void OnIncomingStreamClosed(uint32_t stream_id, bool fin_received) override;

  // Implementation of ContextLifecycleObserver
  void ContextDestroyed(ExecutionContext*) final;

  // Implementation of ActiveScriptWrappable
  bool HasPendingActivity() const final;

  // ScriptWrappable implementation
  void Trace(Visitor* visitor) override;

 private:
  class DatagramUnderlyingSink;
  class DatagramUnderlyingSource;

  void Init(const String& url, ExceptionState&);
  void Dispose();
  void OnConnectionError();

  bool cleanly_closed_ = false;
  Member<ReadableStream> received_datagrams_;
  Member<ReadableStreamDefaultControllerWithScriptScope>
      received_datagrams_controller_;

  // This corresponds to the [[SentDatagrams]] internal slot in the standard.
  Member<WritableStream> outgoing_datagrams_;

  Member<ScriptState> script_state_;

  const KURL url_;
  mojo::Remote<network::mojom::blink::QuicTransport> quic_transport_;
  mojo::Receiver<network::mojom::blink::QuicTransportHandshakeClient>
      handshake_client_receiver_{this};
  mojo::Receiver<network::mojom::blink::QuicTransportClient> client_receiver_{
      this};
  Member<ScriptPromiseResolver> ready_resolver_;
  ScriptPromise ready_;
  Member<ScriptPromiseResolver> closed_resolver_;
  ScriptPromise closed_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_QUIC_TRANSPORT_H_
