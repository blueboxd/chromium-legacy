// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_FRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_FRAME_H_

#include "base/optional.h"
#include "media/base/video_frame.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap_source.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {
class ImageBitmap;
class ExceptionState;
class VideoFrameInit;
class ScriptPromise;
class ScriptState;

class MODULES_EXPORT VideoFrame final : public ScriptWrappable,
                                        public ImageBitmapSource {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit VideoFrame(scoped_refptr<media::VideoFrame> frame);

  // video_frame.idl implementation.
  static VideoFrame* Create(VideoFrameInit*, ImageBitmap*, ExceptionState&);
  ScriptPromise createImageBitmap(ScriptState*,
                                  const ImageBitmapOptions*,
                                  ExceptionState&);

  // ImageBitmapSource implementation
  IntSize BitmapSourceSize() const override;
  ScriptPromise CreateImageBitmap(ScriptState*,
                                  base::Optional<IntRect> crop_rect,
                                  const ImageBitmapOptions*,
                                  ExceptionState&) override;

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
  static constexpr uint64_t kCpuEfficientFrameSize = 480u * 320u;
  bool preferAcceleratedImageBitmap() const;
  scoped_refptr<media::VideoFrame> frame_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_FRAME_H_
