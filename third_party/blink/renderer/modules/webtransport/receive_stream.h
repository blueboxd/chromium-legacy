// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_RECEIVE_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_RECEIVE_STREAM_H_

#include <stdint.h>

#include "mojo/public/cpp/system/data_pipe.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webtransport/incoming_stream.h"

namespace blink {

class ScriptState;
class QuicTransport;

// Implementation of ReceiveStream from the standard:
// https://wicg.github.io/web-transport/#receive-stream.

class MODULES_EXPORT ReceiveStream final : public IncomingStream {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // ReceiveStream doesn't have a JavaScript constructor. It is only constructed
  // from C++.
  explicit ReceiveStream(ScriptState*,
                         QuicTransport*,
                         uint32_t stream_id,
                         mojo::ScopedDataPipeConsumerHandle);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_RECEIVE_STREAM_H_
