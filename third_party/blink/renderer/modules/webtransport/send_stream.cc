// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/send_stream.h"

#include <utility>

#include "base/logging.h"
#include "third_party/blink/renderer/modules/webtransport/quic_transport.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport_close_proxy.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

namespace {

class CloseProxy : public WebTransportCloseProxy {
 public:
  CloseProxy(QuicTransport* quic_transport,
             OutgoingStream* outgoing_stream,
             uint32_t stream_id)
      : quic_transport_(quic_transport),
        outgoing_stream_(outgoing_stream),
        stream_id_(stream_id) {}

  void OnIncomingStreamClosed(bool fin_received) override {
    // OnIncomingStreamClosed only applies to IncomingStreams.
  }

  void SendFin() override { quic_transport_->SendFin(stream_id_); }

  void ForgetStream() override { NOTREACHED(); }

  void Reset() override { outgoing_stream_->Reset(); }

  void Trace(Visitor* visitor) override {
    visitor->Trace(quic_transport_);
    visitor->Trace(outgoing_stream_);
    WebTransportCloseProxy::Trace(visitor);
  }

 private:
  const Member<QuicTransport> quic_transport_;
  const Member<OutgoingStream> outgoing_stream_;
  const uint32_t stream_id_;
};

}  // namespace

SendStream::SendStream(ScriptState* script_state,
                       QuicTransport* quic_transport,
                       uint32_t stream_id,
                       mojo::ScopedDataPipeProducerHandle handle)
    : OutgoingStream(
          script_state,
          MakeGarbageCollected<CloseProxy>(quic_transport, this, stream_id),
          std::move(handle)) {}

}  // namespace blink
