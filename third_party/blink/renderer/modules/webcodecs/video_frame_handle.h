// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_FRAME_HANDLE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_FRAME_HANDLE_H_

#include "base/memory/scoped_refptr.h"
#include "media/base/video_frame.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

namespace blink {

// Wrapper class that allows sharing a single |frame_| reference across
// multiple VideoFrames, which can be invalidated for all frames at once.
class MODULES_EXPORT VideoFrameHandle
    : public WTF::ThreadSafeRefCounted<VideoFrameHandle> {
 public:
  explicit VideoFrameHandle(scoped_refptr<media::VideoFrame>);

  // Returns a copy of |frame_|, which should be re-used throughout the scope
  // of a function call, instead of calling frame() multiple times. Otherwise
  // the frame could be destroyed between calls.
  scoped_refptr<media::VideoFrame> frame();

  // Releases the underlying media::VideoFrame reference, affecting all
  // blink::VideoFrames and blink::Planes that hold a reference to |this|.
  void Invalidate();

 private:
  friend class WTF::ThreadSafeRefCounted<VideoFrameHandle>;
  ~VideoFrameHandle() = default;

  WTF::Mutex mutex_;
  scoped_refptr<media::VideoFrame> frame_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_FRAME_HANDLE_H_
