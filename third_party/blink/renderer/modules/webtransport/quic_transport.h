// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_QUIC_TRANSPORT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_QUIC_TRANSPORT_H_

#include "base/util/type_safety/pass_key.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/quic_transport.mojom-blink.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ScriptState;

class MODULES_EXPORT QuicTransport final
    : public ScriptWrappable,
      public network::mojom::blink::QuicTransportHandshakeClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(QuicTransport, Dispose);

 public:
  using PassKey = util::PassKey<QuicTransport>;
  static QuicTransport* Create(ScriptState* script_state, const String& url);

  QuicTransport(PassKey, ScriptState*, const String& url);
  ~QuicTransport() override = default;

  // QuicTransportHandshakeClient implementation
  void OnConnectionEstablished(
      mojo::PendingRemote<network::mojom::blink::QuicTransport>,
      mojo::PendingReceiver<network::mojom::blink::QuicTransportClient>)
      override;
  void OnHandshakeFailed() override;

  // ScriptWrappable implementation
  void Trace(Visitor* visitor) override;

 private:
  void Init();
  void Dispose();

  const Member<ScriptState> script_state_;
  const KURL url_;
  mojo::Receiver<network::mojom::blink::QuicTransportHandshakeClient>
      handshake_client_receiver_;
};

}  // namespace blink

#endif
