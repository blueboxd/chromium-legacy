// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ENCODED_AUDIO_UNDERLYING_SINK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ENCODED_AUDIO_UNDERLYING_SINK_H_

#include "third_party/blink/renderer/core/streams/underlying_sink_base.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class ExceptionState;
class RTCEncodedAudioStreamTransformer;

class MODULES_EXPORT RTCEncodedAudioUnderlyingSink final
    : public UnderlyingSinkBase {
 public:
  using TransformerCallback =
      base::RepeatingCallback<RTCEncodedAudioStreamTransformer*()>;
  RTCEncodedAudioUnderlyingSink(ScriptState*, TransformerCallback);

  // UnderlyingSinkBase
  ScriptPromise start(ScriptState*,
                      WritableStreamDefaultController*,
                      ExceptionState&) override;
  ScriptPromise write(ScriptState*,
                      ScriptValue chunk,
                      WritableStreamDefaultController*,
                      ExceptionState&) override;
  ScriptPromise close(ScriptState*, ExceptionState&) override;
  ScriptPromise abort(ScriptState*,
                      ScriptValue reason,
                      ExceptionState&) override;

  void Trace(Visitor*) override;

 private:
  TransformerCallback transformer_callback_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ENCODED_AUDIO_UNDERLYING_SINK_H_
