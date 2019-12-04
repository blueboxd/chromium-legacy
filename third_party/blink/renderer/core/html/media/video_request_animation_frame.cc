// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/video_request_animation_frame.h"

#include "third_party/blink/renderer/core/html/media/html_video_element.h"

namespace blink {

VideoRequestAnimationFrame::VideoRequestAnimationFrame(
    HTMLVideoElement& element)
    : Supplement<HTMLVideoElement>(element) {}

// static
VideoRequestAnimationFrame* VideoRequestAnimationFrame::From(
    HTMLVideoElement& element) {
  return Supplement<HTMLVideoElement>::From<VideoRequestAnimationFrame>(
      element);
}

void VideoRequestAnimationFrame::Trace(blink::Visitor* visitor) {
  Supplement<HTMLVideoElement>::Trace(visitor);
}

// static
const char VideoRequestAnimationFrame::kSupplementName[] =
    "VideoRequestAnimationFrame";

}  // namespace blink
