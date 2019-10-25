// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_QUIC_TRANSPORT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_QUIC_TRANSPORT_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ScriptState;

class MODULES_EXPORT QuicTransport final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(QuicTransport, Dispose);

 public:
  static QuicTransport* Create(ScriptState* script_state, const String& url) {
    return MakeGarbageCollected<QuicTransport>(script_state, url);
  }

  QuicTransport(ScriptState*, const String& url);
  ~QuicTransport() override = default;

 private:
  void Dispose();
};

}  // namespace blink

#endif
