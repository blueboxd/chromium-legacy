// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_VIDEO_REQUEST_ANIMATION_FRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_VIDEO_REQUEST_ANIMATION_FRAME_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace media {
class VideoFrame;
}

namespace blink {

class HTMLVideoElement;

// Interface that defines the interfaces that HTMLVideoElement should be aware
// of in order to support the <video>.requestAnimationFrame() API.
class CORE_EXPORT VideoRequestAnimationFrame
    : public GarbageCollected<VideoRequestAnimationFrame>,
      public Supplement<HTMLVideoElement> {
  USING_GARBAGE_COLLECTED_MIXIN(VideoRequestAnimationFrame);

 public:
  static const char kSupplementName[];

  static VideoRequestAnimationFrame* From(HTMLVideoElement&);

  virtual ~VideoRequestAnimationFrame() = default;

  void Trace(blink::Visitor*) override;

  virtual void OnWebMediaPlayerCreated() = 0;
  virtual void OnRequestAnimationFrame(
      base::TimeTicks presentation_time,
      base::TimeTicks expected_presentation_time,
      uint32_t presented_frames_counter,
      const media::VideoFrame&) = 0;

 protected:
  explicit VideoRequestAnimationFrame(HTMLVideoElement&);
  DISALLOW_COPY_AND_ASSIGN(VideoRequestAnimationFrame);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_VIDEO_REQUEST_ANIMATION_FRAME_H_
