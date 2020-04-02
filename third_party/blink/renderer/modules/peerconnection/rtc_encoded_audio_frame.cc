// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame.h"

#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"

namespace blink {

RTCEncodedAudioFrame::RTCEncodedAudioFrame(
    std::unique_ptr<webrtc::TransformableFrameInterface> webrtc_frame)
    : webrtc_frame_(std::move(webrtc_frame)) {}

RTCEncodedAudioFrame::RTCEncodedAudioFrame(
    std::unique_ptr<webrtc::TransformableAudioFrameInterface>
        webrtc_audio_frame) {
  if (webrtc_audio_frame) {
    wtf_size_t num_csrcs = webrtc_audio_frame->GetHeader().numCSRCs;
    contributing_sources_.ReserveInitialCapacity(num_csrcs);
    for (wtf_size_t i = 0; i < num_csrcs; i++) {
      contributing_sources_.push_back(
          webrtc_audio_frame->GetHeader().arrOfCSRCs[i]);
    }
  }
  webrtc_frame_ = std::move(webrtc_audio_frame);
}

uint64_t RTCEncodedAudioFrame::timestamp() const {
  return webrtc_frame_ ? webrtc_frame_->GetTimestamp() : 0;
}

DOMArrayBuffer* RTCEncodedAudioFrame::data() const {
  if (webrtc_frame_ && !frame_data_) {
    frame_data_ = DOMArrayBuffer::Create(webrtc_frame_->GetData().data(),
                                         webrtc_frame_->GetData().size());
  }
  return frame_data_;
}

DOMArrayBuffer* RTCEncodedAudioFrame::additionalData() const {
  return nullptr;
}

void RTCEncodedAudioFrame::setData(DOMArrayBuffer* data) {
  frame_data_ = data;
}

uint32_t RTCEncodedAudioFrame::synchronizationSource() const {
  return webrtc_frame_ ? webrtc_frame_->GetSsrc() : 0;
}

Vector<uint32_t> RTCEncodedAudioFrame::contributingSources() const {
  return webrtc_frame_ ? contributing_sources_ : Vector<uint32_t>();
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

std::unique_ptr<webrtc::TransformableFrameInterface>
RTCEncodedAudioFrame::PassDelegate() {
  // Sync the delegate data with |frame_data_| if necessary.
  if (webrtc_frame_ && frame_data_) {
    webrtc_frame_->SetData(rtc::ArrayView<const uint8_t>(
        static_cast<const uint8_t*>(frame_data_->Data()),
        frame_data_->ByteLengthAsSizeT()));
  }
  return std::move(webrtc_frame_);
}

void RTCEncodedAudioFrame::Trace(Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(frame_data_);
}

}  // namespace blink
