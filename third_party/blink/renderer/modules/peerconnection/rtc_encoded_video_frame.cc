// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_frame.h"

#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/webrtc/api/video/encoded_frame.h"
#include "third_party/webrtc/api/video/encoded_image.h"

namespace blink {

RTCEncodedVideoFrame::RTCEncodedVideoFrame(
    std::unique_ptr<webrtc::video_coding::EncodedFrame> delegate,
    Vector<uint8_t> additional_data,
    uint32_t ssrc)
    : delegate_(std::move(delegate)),
      additional_data_vector_(std::move(additional_data)),
      ssrc_(ssrc) {}

String RTCEncodedVideoFrame::type() const {
  if (!delegate_)
    return "empty";

  return delegate_->is_keyframe() ? "key" : "delta";
}

uint64_t RTCEncodedVideoFrame::timestamp() const {
  return delegate_ ? delegate_->Timestamp() : 0;
}

DOMArrayBuffer* RTCEncodedVideoFrame::data() const {
  if (delegate_ && !frame_data_) {
    frame_data_ = DOMArrayBuffer::Create(delegate_->EncodedImage().data(),
                                         delegate_->EncodedImage().size());
  }
  return frame_data_;
}

DOMArrayBuffer* RTCEncodedVideoFrame::additionalData() const {
  if (!additional_data_) {
    additional_data_ = DOMArrayBuffer::Create(additional_data_vector_.data(),
                                              additional_data_vector_.size());
  }
  return additional_data_;
}

uint32_t RTCEncodedVideoFrame::synchronizationSource() const {
  return ssrc_;
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

std::unique_ptr<webrtc::video_coding::EncodedFrame>
RTCEncodedVideoFrame::PassDelegate() {
  // Sync the delegate data with |frame_data_| if necessary.
  if (delegate_ && frame_data_) {
    rtc::scoped_refptr<webrtc::EncodedImageBuffer> webrtc_image =
        webrtc::EncodedImageBuffer::Create(
            static_cast<const uint8_t*>(frame_data_->Data()),
            frame_data_->ByteLengthAsSizeT());
    delegate_->SetEncodedData(std::move(webrtc_image));
  }
  return std::move(delegate_);
}

void RTCEncodedVideoFrame::Trace(Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(frame_data_);
  visitor->Trace(additional_data_);
}

}  // namespace blink
