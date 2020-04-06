// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_FRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_FRAME_H_

#include "base/optional.h"
#include "media/base/video_frame.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
class ImageBitmap;
class ExceptionState;
class VideoFrameInit;

class MODULES_EXPORT VideoFrame final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit VideoFrame(scoped_refptr<media::VideoFrame> frame);

  // video_frame.idl implementation.
  static VideoFrame* Create(VideoFrameInit*, ImageBitmap*, ExceptionState&);

  uint64_t timestamp() const;
  base::Optional<uint64_t> duration() const;

  uint32_t codedWidth() const;
  uint32_t codedHeight() const;
  uint32_t visibleWidth() const;
  uint32_t visibleHeight() const;

  void release();

  // Convenience functions
  scoped_refptr<media::VideoFrame> frame();
  scoped_refptr<const media::VideoFrame> frame() const;

 private:
  scoped_refptr<media::VideoFrame> frame_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_FRAME_H_
