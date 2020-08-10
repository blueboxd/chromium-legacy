// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_FRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_FRAME_H_

#include <stdint.h>

#include "base/optional.h"
#include "media/base/video_frame.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap_source.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ImageBitmap;
class ExceptionState;
class Plane;
class ScriptPromise;
class ScriptState;
class VideoFrameInit;

class MODULES_EXPORT VideoFrame final : public ScriptWrappable,
                                        public ImageBitmapSource {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit VideoFrame(scoped_refptr<media::VideoFrame> frame);

  // video_frame.idl implementation.
  static VideoFrame* Create(ImageBitmap*, VideoFrameInit*, ExceptionState&);

  String format() const;
  HeapVector<Member<Plane>> planes() const;

  uint32_t codedWidth() const;
  uint32_t codedHeight() const;

  uint32_t cropLeft() const;
  uint32_t cropTop() const;
  uint32_t cropWidth() const;
  uint32_t cropHeight() const;

  uint32_t displayWidth() const;
  uint32_t displayHeight() const;

  base::Optional<uint64_t> timestamp() const;
  base::Optional<uint64_t> duration() const;

  void close();

  ScriptPromise createImageBitmap(ScriptState*,
                                  const ImageBitmapOptions*,
                                  ExceptionState&);

  // Convenience functions
  scoped_refptr<media::VideoFrame> frame();
  scoped_refptr<const media::VideoFrame> frame() const;

 private:
  // ImageBitmapSource implementation
  static constexpr uint64_t kCpuEfficientFrameSize = 320u * 240u;
  IntSize BitmapSourceSize() const override;
  bool preferAcceleratedImageBitmap() const;
  ScriptPromise CreateImageBitmap(ScriptState*,
                                  base::Optional<IntRect> crop_rect,
                                  const ImageBitmapOptions*,
                                  ExceptionState&) override;

  scoped_refptr<media::VideoFrame> frame_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_FRAME_H_
