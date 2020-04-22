// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/receive_stream.h"

#include <utility>

#include "third_party/blink/renderer/modules/webtransport/quic_transport.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

ReceiveStream::ReceiveStream(ScriptState* script_state,
                             QuicTransport* quic_transport,
                             uint32_t stream_id,
                             mojo::ScopedDataPipeConsumerHandle handle)
    : incoming_stream_(MakeGarbageCollected<IncomingStream>(
          script_state,
          WTF::Bind(&ReceiveStream::ForgetStream, WrapWeakPersistent(this)),
          std::move(handle))),
      quic_transport_(quic_transport),
      stream_id_(stream_id) {}

void ReceiveStream::OnIncomingStreamClosed(bool fin_received) {
  incoming_stream_->OnIncomingStreamClosed(fin_received);
}

void ReceiveStream::Reset() {
  incoming_stream_->Reset();
}

void ReceiveStream::ContextDestroyed() {
  incoming_stream_->ContextDestroyed();
}

void ReceiveStream::Trace(Visitor* visitor) {
  visitor->Trace(incoming_stream_);
  visitor->Trace(quic_transport_);
  ScriptWrappable::Trace(visitor);
  WebTransportStream::Trace(visitor);
}

void ReceiveStream::ForgetStream() {
  quic_transport_->ForgetStream(stream_id_);
}

}  // namespace blink
