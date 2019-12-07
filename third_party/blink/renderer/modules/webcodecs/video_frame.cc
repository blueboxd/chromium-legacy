// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"

#include <utility>

#include "media/base/video_frame.h"

namespace blink {

VideoFrame::VideoFrame(scoped_refptr<media::VideoFrame> frame)
    : frame_(std::move(frame)) {
  DCHECK(frame_);
}

scoped_refptr<media::VideoFrame> VideoFrame::frame() {
  return frame_;
}

scoped_refptr<const media::VideoFrame> VideoFrame::frame() const {
  return frame_;
}

uint64_t VideoFrame::timestamp() const {
  if (!frame_)
    return 0;
  return frame_->timestamp().InMicroseconds();
}

uint32_t VideoFrame::coded_width() const {
  if (!frame_)
    return 0;
  return frame_->coded_size().width();
}

uint32_t VideoFrame::coded_height() const {
  if (!frame_)
    return 0;
  return frame_->coded_size().height();
}

uint32_t VideoFrame::visible_width() const {
  if (!frame_)
    return 0;
  return frame_->visible_rect().width();
}

uint32_t VideoFrame::visible_height() const {
  if (!frame_)
    return 0;
  return frame_->visible_rect().height();
}

void VideoFrame::release() {
  frame_.reset();
}

}  // namespace blink
