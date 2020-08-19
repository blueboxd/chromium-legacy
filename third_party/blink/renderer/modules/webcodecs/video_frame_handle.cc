// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_frame_handle.h"

namespace blink {

VideoFrameHandle::VideoFrameHandle(scoped_refptr<media::VideoFrame> frame)
    : frame_(std::move(frame)) {
  DCHECK(frame_);
}

scoped_refptr<media::VideoFrame> VideoFrameHandle::frame() {
  WTF::MutexLocker locker(mutex_);
  return frame_;
}

void VideoFrameHandle::Invalidate() {
  WTF::MutexLocker locker(mutex_);
  frame_.reset();
}

}  // namespace blink
