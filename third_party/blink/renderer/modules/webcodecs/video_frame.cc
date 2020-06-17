// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"

#include <utility>

#include "media/base/video_frame.h"
#include "media/base/video_frame_metadata.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame_init.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap_factories.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/libyuv/include/libyuv.h"

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

base::Optional<uint64_t> VideoFrame::duration() const {
  base::TimeDelta result;
  if (frame_ && frame_->metadata()->GetTimeDelta(
                    media::VideoFrameMetadata::FRAME_DURATION, &result)) {
    return result.InMicroseconds();
  }
  return base::Optional<uint64_t>();
}

uint32_t VideoFrame::codedWidth() const {
  if (!frame_)
    return 0;
  return frame_->coded_size().width();
}

uint32_t VideoFrame::codedHeight() const {
  if (!frame_)
    return 0;
  return frame_->coded_size().height();
}

uint32_t VideoFrame::visibleWidth() const {
  if (!frame_)
    return 0;
  return frame_->visible_rect().width();
}

uint32_t VideoFrame::visibleHeight() const {
  if (!frame_)
    return 0;
  return frame_->visible_rect().height();
}

void VideoFrame::release() {
  frame_.reset();
}

// static
VideoFrame* VideoFrame::Create(VideoFrameInit* init,
                               ImageBitmap* source,
                               ExceptionState& exception_state) {
  if (!source) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotFoundError,
                                      "No source was provided");
    return nullptr;
  }
  gfx::Size size(source->width(), source->height());
  gfx::Rect rect(size);
  base::TimeDelta timestamp =
      base::TimeDelta::FromMicroseconds(init->timestamp());

  if (!source) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid source state");
    return nullptr;
  }

  // Full copy #1
  WTF::Vector<uint8_t> pixel_data = source->CopyBitmapData();
  if (pixel_data.size() <
      media::VideoFrame::AllocationSize(media::PIXEL_FORMAT_ARGB, size)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kBufferOverrunError,
                                      "Image buffer is too small.");
    return nullptr;
  }

  auto frame = media::VideoFrame::CreateFrame(media::PIXEL_FORMAT_I420, size,
                                              rect, size, timestamp);
  if (!frame) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Frame creation failed");
    return nullptr;
  }

  // Full copy #2
  int error =
      libyuv::ABGRToI420(pixel_data.data(), source->width() * 4,
                         frame->visible_data(media::VideoFrame::kYPlane),
                         frame->stride(media::VideoFrame::kYPlane),
                         frame->visible_data(media::VideoFrame::kUPlane),
                         frame->stride(media::VideoFrame::kUPlane),
                         frame->visible_data(media::VideoFrame::kVPlane),
                         frame->stride(media::VideoFrame::kVPlane),
                         source->width(), source->height());
  if (error) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "ARGB to YUV420 conversion error");
    return nullptr;
  }
  // TODO(jie.a.chen@intel.com): Figure out the right colorspace and conversion
  // according to source ImageBitmap.
  // libyuv::ABGRToI420 seems to assume Bt.601.
  frame->set_color_space(gfx::ColorSpace::CreateREC601());
  auto* result = MakeGarbageCollected<VideoFrame>(std::move(frame));
  return result;
}

ScriptPromise VideoFrame::createImageBitmap(ScriptState* script_state,
                                            const ImageBitmapOptions* options,
                                            ExceptionState& exception_state) {
  return ImageBitmapFactories::CreateImageBitmap(
      script_state, this, base::Optional<IntRect>(), options, exception_state);
}

IntSize VideoFrame::BitmapSourceSize() const {
  return IntSize(visibleWidth(), visibleHeight());
}

ScriptPromise VideoFrame::CreateImageBitmap(ScriptState* script_state,
                                            base::Optional<IntRect> crop_rect,
                                            const ImageBitmapOptions* options,
                                            ExceptionState& exception_state) {
  if (frame_->IsMappable() && !frame_->HasTextures()) {
    size_t bytes_per_row = sizeof(SkColor) * visibleWidth();
    size_t image_pixels_size = bytes_per_row * visibleHeight();

    sk_sp<SkData> image_pixels = TryAllocateSkData(image_pixels_size);
    if (!image_pixels) {
      exception_state.ThrowDOMException(DOMExceptionCode::kBufferOverrunError,
                                        "Out of memory.");
      return ScriptPromise();
    }
    media::PaintCanvasVideoRenderer::ConvertVideoFrameToRGBPixels(
        frame_.get(), image_pixels->writable_data(), bytes_per_row);

    // TODO(jie.a.chen@intel.com): Figure the correct SkColorSpace.
    sk_sp<SkColorSpace> skColorSpace = SkColorSpace::MakeSRGB();
    SkImageInfo info =
        SkImageInfo::Make(visibleWidth(), visibleHeight(), kN32_SkColorType,
                          kUnpremul_SkAlphaType, std::move(skColorSpace));
    sk_sp<SkImage> skImage =
        SkImage::MakeRasterData(info, image_pixels, bytes_per_row);
    scoped_refptr<StaticBitmapImage> image =
        UnacceleratedStaticBitmapImage::Create(std::move(skImage));
    return ImageBitmapSource::FulfillImageBitmap(
        script_state,
        MakeGarbageCollected<ImageBitmap>(std::move(image), crop_rect, options),
        exception_state);
  }

  // TODO(jie.a.chen@intel.com): AcceleratedStaticImageBitmap for hardware
  // video frames.
  exception_state.ThrowDOMException(
      DOMExceptionCode::kNotSupportedError,
      "Hardware accelerated video not supported.");
  return ScriptPromise();
}

}  // namespace blink
