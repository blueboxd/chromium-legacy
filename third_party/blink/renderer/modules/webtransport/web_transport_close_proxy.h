// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_WEB_TRANSPORT_CLOSE_PROXY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_WEB_TRANSPORT_CLOSE_PROXY_H_

#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

class Visitor;

// This is an internal proxy type for passing close messages between
// QuicTransport and the IncomingStream and OutgoingStream mixins. It is not
// part of the standard. It exists to abstract over the fact that a single
// stream_id can correspond to one or two JavaScript streams.
class WebTransportCloseProxy : public GarbageCollected<WebTransportCloseProxy> {
 public:
  virtual ~WebTransportCloseProxy() = default;

  // These match the mojo interfaces, but without the stream_id argument.

  // Process an IncomingStreamClosed message from the network service. This is
  // called by QuicTransport objects. May execute user JavaScript.
  virtual void OnIncomingStreamClosed(bool fin_received) = 0;

  // Send a Fin signal to the network service. This is used by OutgoingStream.
  virtual void SendFin() = 0;

  // Cause QuicTransport to drop reference to a stream. This is used by
  // IncomingStream.
  virtual void ForgetStream() = 0;

  // Called from QuicTransport whenever the mojo connection is torn down. Should
  // close and free data pipes. May execute user JavaScript.
  virtual void Reset() = 0;

  virtual void Trace(Visitor*) {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_WEB_TRANSPORT_CLOSE_PROXY_H_
