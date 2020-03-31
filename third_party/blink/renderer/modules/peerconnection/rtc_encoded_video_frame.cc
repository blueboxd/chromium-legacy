// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_frame.h"

#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"

namespace blink {

RTCEncodedVideoFrame::RTCEncodedVideoFrame(
    std::unique_ptr<webrtc::TransformableVideoFrameInterface> delegate)
    : delegate_(std::move(delegate)) {}

String RTCEncodedVideoFrame::type() const {
  if (!delegate_)
    return "empty";

  return delegate_->IsKeyFrame() ? "key" : "delta";
}

uint64_t RTCEncodedVideoFrame::timestamp() const {
  return delegate_ ? delegate_->GetTimestamp() : 0;
}

DOMArrayBuffer* RTCEncodedVideoFrame::data() const {
  if (delegate_ && !frame_data_) {
    frame_data_ = DOMArrayBuffer::Create(delegate_->GetData().data(),
                                         delegate_->GetData().size());
  }
  return frame_data_;
}

DOMArrayBuffer* RTCEncodedVideoFrame::additionalData() const {
  if (delegate_ && !additional_data_) {
    auto additional_data = delegate_->GetAdditionalData();
    additional_data_ =
        DOMArrayBuffer::Create(additional_data.data(), additional_data.size());
  }
  return additional_data_;
}

uint32_t RTCEncodedVideoFrame::synchronizationSource() const {
  return delegate_ ? delegate_->GetSsrc() : 0;
}

void RTCEncodedVideoFrame::setData(DOMArrayBuffer* data) {
  frame_data_ = data;
}

String RTCEncodedVideoFrame::toString() const {
  if (!delegate_)
    return "empty";

  StringBuilder sb;
  sb.Append("RTCEncodedVideoFrame{timestamp: ");
  sb.AppendNumber(timestamp());
  sb.Append("us, size: ");
  sb.AppendNumber(data()->ByteLengthAsSizeT());
  sb.Append(" bytes, type: ");
  sb.Append(type());
  sb.Append("}");
  return sb.ToString();
}

std::unique_ptr<webrtc::TransformableVideoFrameInterface>
RTCEncodedVideoFrame::PassDelegate() {
  // Sync the delegate data with |frame_data_| if necessary.
  if (delegate_ && frame_data_) {
    delegate_->SetData(rtc::ArrayView<const uint8_t>(
        static_cast<const uint8_t*>(frame_data_->Data()),
        frame_data_->ByteLengthAsSizeT()));
  }
  return std::move(delegate_);
}

void RTCEncodedVideoFrame::Trace(Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(frame_data_);
  visitor->Trace(additional_data_);
}

}  // namespace blink
