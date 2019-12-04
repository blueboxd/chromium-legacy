// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_VIDEO_RAF_VIDEO_REQUEST_ANIMATION_FRAME_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_VIDEO_RAF_VIDEO_REQUEST_ANIMATION_FRAME_IMPL_H_

#include "media/base/video_frame.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/media/video_request_animation_frame.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/video_raf/video_frame_request_callback_collection.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class HTMLVideoElement;

// Implementation of the <video>.requestAnimationFrame() API.
// Extends HTMLVideoElement via the VideoRequestAnimationFrame interface.
class MODULES_EXPORT VideoRequestAnimationFrameImpl final
    : public VideoRequestAnimationFrame {
  USING_GARBAGE_COLLECTED_MIXIN(VideoRequestAnimationFrameImpl);

 public:
  static VideoRequestAnimationFrameImpl& From(HTMLVideoElement&);

  // Web API entry points for requestAnimationFrame().
  static int requestAnimationFrame(HTMLVideoElement&,
                                   V8VideoFrameRequestCallback*);
  static void cancelAnimationFrame(HTMLVideoElement&, int);

  explicit VideoRequestAnimationFrameImpl(HTMLVideoElement&);
  ~VideoRequestAnimationFrameImpl() override = default;

  void Trace(blink::Visitor*) override;

  int requestAnimationFrame(V8VideoFrameRequestCallback*);
  void cancelAnimationFrame(int);

  void OnWebMediaPlayerCreated() override;
  void OnRequestAnimationFrame(base::TimeTicks presentation_time,
                               base::TimeTicks expected_presentation_time,
                               uint32_t presented_frames_counter,
                               const media::VideoFrame&) override;

 private:
  Member<VideoFrameRequestCallbackCollection> callback_collection_;

  DISALLOW_COPY_AND_ASSIGN(VideoRequestAnimationFrameImpl);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_VIDEO_RAF_VIDEO_REQUEST_ANIMATION_FRAME_IMPL_H_
