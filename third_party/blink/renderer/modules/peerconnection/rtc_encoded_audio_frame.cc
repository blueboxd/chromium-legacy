// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame.h"

#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

uint64_t RTCEncodedAudioFrame::timestamp() const {
  return 0;
}

DOMArrayBuffer* RTCEncodedAudioFrame::data() const {
  return frame_data_;
}

DOMArrayBuffer* RTCEncodedAudioFrame::additionalData() const {
  return nullptr;
}

void RTCEncodedAudioFrame::setData(DOMArrayBuffer* data) {
  frame_data_ = data;
}

uint32_t RTCEncodedAudioFrame::synchronizationSource() const {
  return 0;
}

Vector<uint32_t> RTCEncodedAudioFrame::contributingSources() const {
  return Vector<uint32_t>();
}

String RTCEncodedAudioFrame::toString() const {
  StringBuilder sb;
  sb.Append("RTCEncodedAudioFrame{timestamp: ");
  sb.AppendNumber(timestamp());
  sb.Append("us, size: ");
  sb.AppendNumber(data() ? data()->ByteLengthAsSizeT() : 0);
  sb.Append("}");
  return sb.ToString();
}

void RTCEncodedAudioFrame::Trace(Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(frame_data_);
}

}  // namespace blink
