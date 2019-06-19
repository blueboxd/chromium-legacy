// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/modules/webrtc/webrtc_video_frame_adapter.h"

#include "base/logging.h"
#include "third_party/webrtc/common_video/include/video_frame_buffer.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

namespace {

template <typename Base>
class FrameAdapter : public Base {
 public:
  explicit FrameAdapter(scoped_refptr<media::VideoFrame> frame)
      : frame_(std::move(frame)) {}

  int width() const override { return frame_->visible_rect().width(); }
  int height() const override { return frame_->visible_rect().height(); }

  const uint8_t* DataY() const override {
    return frame_->visible_data(media::VideoFrame::kYPlane);
  }

  const uint8_t* DataU() const override {
    return frame_->visible_data(media::VideoFrame::kUPlane);
  }

  const uint8_t* DataV() const override {
    return frame_->visible_data(media::VideoFrame::kVPlane);
  }

  int StrideY() const override {
    return frame_->stride(media::VideoFrame::kYPlane);
  }

  int StrideU() const override {
    return frame_->stride(media::VideoFrame::kUPlane);
  }

  int StrideV() const override {
    return frame_->stride(media::VideoFrame::kVPlane);
  }

 protected:
  scoped_refptr<media::VideoFrame> frame_;
};

template <typename BaseWithA>
class FrameAdapterWithA : public FrameAdapter<BaseWithA> {
 public:
  explicit FrameAdapterWithA(scoped_refptr<media::VideoFrame> frame)
      : FrameAdapter<BaseWithA>(std::move(frame)) {}

  const uint8_t* DataA() const override {
    return FrameAdapter<BaseWithA>::frame_->visible_data(
        media::VideoFrame::kAPlane);
  }

  int StrideA() const override {
    return FrameAdapter<BaseWithA>::frame_->stride(media::VideoFrame::kAPlane);
  }
};

void IsValidFrame(const media::VideoFrame& frame) {
  // Paranoia checks.
  DCHECK(media::VideoFrame::IsValidConfig(
      frame.format(), frame.storage_type(), frame.coded_size(),
      frame.visible_rect(), frame.natural_size()));
  DCHECK(media::PIXEL_FORMAT_I420 == frame.format() ||
         media::PIXEL_FORMAT_I420A == frame.format());
  CHECK(reinterpret_cast<const void*>(frame.data(media::VideoFrame::kYPlane)));
  CHECK(reinterpret_cast<const void*>(frame.data(media::VideoFrame::kUPlane)));
  CHECK(reinterpret_cast<const void*>(frame.data(media::VideoFrame::kVPlane)));
  CHECK(frame.stride(media::VideoFrame::kYPlane));
  CHECK(frame.stride(media::VideoFrame::kUPlane));
  CHECK(frame.stride(media::VideoFrame::kVPlane));
}

}  // anonymous namespace

namespace blink {

WebRtcVideoFrameAdapter::WebRtcVideoFrameAdapter(
    scoped_refptr<media::VideoFrame> frame)
    : frame_(std::move(frame)) {}

WebRtcVideoFrameAdapter::~WebRtcVideoFrameAdapter() {}

webrtc::VideoFrameBuffer::Type WebRtcVideoFrameAdapter::type() const {
  return Type::kNative;
}

int WebRtcVideoFrameAdapter::width() const {
  return frame_->visible_rect().width();
}

int WebRtcVideoFrameAdapter::height() const {
  return frame_->visible_rect().height();
}

rtc::scoped_refptr<webrtc::I420BufferInterface>
WebRtcVideoFrameAdapter::CreateFrameAdapter() const {
  // We cant convert texture synchronously due to threading issues, see
  // https://crbug.com/663452. Instead, return a black frame (yuv = {0, 0x80,
  // 0x80}).
  if (frame_->HasTextures()) {
    DLOG(ERROR) << "Texture backed frame cannot be accessed.";
    return new rtc::RefCountedObject<FrameAdapter<webrtc::I420BufferInterface>>(
        media::VideoFrame::CreateColorFrame(frame_->visible_rect().size(), 0u,
                                            0x80, 0x80, frame_->timestamp()));
  }

  IsValidFrame(*frame_);
  if (media::PIXEL_FORMAT_I420A == frame_->format()) {
    return new rtc::RefCountedObject<
        FrameAdapterWithA<webrtc::I420ABufferInterface>>(frame_);
  }
  return new rtc::RefCountedObject<FrameAdapter<webrtc::I420BufferInterface>>(
      frame_);
}

rtc::scoped_refptr<webrtc::I420BufferInterface>
WebRtcVideoFrameAdapter::ToI420() {
  if (!frame_adapter_) {
    frame_adapter_ = CreateFrameAdapter();
  }
  return frame_adapter_;
}

const webrtc::I420BufferInterface* WebRtcVideoFrameAdapter::GetI420() const {
  if (!frame_adapter_) {
    frame_adapter_ = CreateFrameAdapter();
  }
  return frame_adapter_.get();
}

}  // namespace blink
