// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_VIDEO_RAF_VIDEO_REQUEST_ANIMATION_FRAME_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_VIDEO_RAF_VIDEO_REQUEST_ANIMATION_FRAME_IMPL_H_

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

  void Trace(Visitor*) override;

  int requestAnimationFrame(V8VideoFrameRequestCallback*);
  void cancelAnimationFrame(int);

  void OnWebMediaPlayerCreated() override;
  void OnRequestAnimationFrame() override;

  void ExecuteFrameCallbacks(double high_res_now_ms);

 private:
  friend class VideoRequestAnimationFrameImplTest;

  // Register a non-V8 callback for testing. Also sets |pending_execution_| to
  // true, to allow calling into ExecuteFrameCallbacks() directly.
  void RegisterCallbackForTest(
      VideoFrameRequestCallbackCollection::VideoFrameCallback*);

  // Used to keep track of whether or not we have already scheduled a call to
  // ExecuteFrameCallbacks() in the next rendering steps.
  bool pending_execution_ = false;

  Member<VideoFrameRequestCallbackCollection> callback_collection_;

  DISALLOW_COPY_AND_ASSIGN(VideoRequestAnimationFrameImpl);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_VIDEO_RAF_VIDEO_REQUEST_ANIMATION_FRAME_IMPL_H_
