// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ENCODED_AUDIO_FRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ENCODED_AUDIO_FRAME_H_

#include <stdint.h>

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class DOMArrayBuffer;

class RTCEncodedAudioFrame final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  RTCEncodedAudioFrame() = default;

  // rtc_encoded_audio_frame.idl implementation.
  uint64_t timestamp() const;
  DOMArrayBuffer* data() const;
  DOMArrayBuffer* additionalData() const;
  void setData(DOMArrayBuffer*);
  uint32_t synchronizationSource() const;
  Vector<uint32_t> contributingSources() const;
  String toString() const;

  void Trace(Visitor*) override;

 private:
  mutable Member<DOMArrayBuffer> frame_data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ENCODED_AUDIO_FRAME_H_
