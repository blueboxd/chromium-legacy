// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/receive_stream.h"

#include <utility>

#include "third_party/blink/renderer/modules/webtransport/quic_transport.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport_close_proxy.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

namespace {

class CloseProxy : public WebTransportCloseProxy {
 public:
  CloseProxy(QuicTransport* quic_transport,
             IncomingStream* incoming_stream,
             uint32_t stream_id)
      : quic_transport_(quic_transport),
        incoming_stream_(incoming_stream),
        stream_id_(stream_id) {}

  void OnIncomingStreamClosed(bool fin_received) override {
    incoming_stream_->OnIncomingStreamClosed(fin_received);
  }

  void SendFin() override { NOTREACHED(); }

  void ForgetStream() override { quic_transport_->ForgetStream(stream_id_); }

  void Reset() override { incoming_stream_->Reset(); }

  void Trace(Visitor* visitor) override {
    visitor->Trace(quic_transport_);
    visitor->Trace(incoming_stream_);
    WebTransportCloseProxy::Trace(visitor);
  }

 private:
  const Member<QuicTransport> quic_transport_;
  const Member<IncomingStream> incoming_stream_;
  const uint32_t stream_id_;
};

}  // namespace

ReceiveStream::ReceiveStream(ScriptState* script_state,
                             QuicTransport* quic_transport,
                             uint32_t stream_id,
                             mojo::ScopedDataPipeConsumerHandle handle)
    : IncomingStream(
          script_state,
          MakeGarbageCollected<CloseProxy>(quic_transport, this, stream_id),
          std::move(handle)) {}

}  // namespace blink
