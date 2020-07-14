// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"

#include <utility>

#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_metadata.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "media/renderers/yuv_util.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_frame_init.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap_factories.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/libyuv/include/libyuv.h"

namespace blink {

namespace {

bool IsValidSkColorSpace(sk_sp<SkColorSpace> sk_color_space) {
  // Refer to CanvasColorSpaceToGfxColorSpace in CanvasColorParams.
  sk_sp<SkColorSpace> valid_sk_color_spaces[] = {
      gfx::ColorSpace::CreateSRGB().ToSkColorSpace(),
      gfx::ColorSpace::CreateDisplayP3D65().ToSkColorSpace(),
      gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                      gfx::ColorSpace::TransferID::GAMMA24)
          .ToSkColorSpace()};
  for (auto& valid_sk_color_space : valid_sk_color_spaces) {
    if (SkColorSpace::Equals(sk_color_space.get(),
                             valid_sk_color_space.get())) {
      return true;
    }
  }
  return false;
}

bool IsValidSkColorType(SkColorType sk_color_type) {
  SkColorType valid_sk_color_types[] = {
      kBGRA_8888_SkColorType, kRGBA_8888_SkColorType,
      // TODO(jie.a.chen@intel.com): Add F16 support.
      // kRGBA_F16_SkColorType
  };
  for (auto& valid_sk_color_type : valid_sk_color_types) {
    if (sk_color_type == valid_sk_color_type) {
      return true;
    }
  }
  return false;
}

}  // namespace

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
  if (frame_ && frame_->metadata()->frame_duration.has_value())
    return frame_->metadata()->frame_duration->InMicroseconds();

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

  auto sk_image =
      source->BitmapImage()->PaintImageForCurrentFrame().GetSkImage();
  auto sk_color_space = sk_image->refColorSpace();
  if (!sk_color_space) {
    sk_color_space = SkColorSpace::MakeSRGB();
  }
  if (!IsValidSkColorSpace(sk_color_space)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid color space");
    return nullptr;
  }
  auto sk_color_type = sk_image->colorType();
  if (!IsValidSkColorType(sk_color_type)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Invalid pixel format");
    return nullptr;
  }

  // TODO(jie.a.chen@intel.com): Handle data of float type.
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

#if SK_PMCOLOR_BYTE_ORDER(B, G, R, A)
  auto libyuv_convert_to_i420 = libyuv::ARGBToI420;
#else
  auto libyuv_convert_to_i420 = libyuv::ABGRToI420;
#endif
  if (sk_color_type != kN32_SkColorType) {
    // Swap ARGB and ABGR if not using the native pixel format.
    libyuv_convert_to_i420 =
        (libyuv_convert_to_i420 == libyuv::ARGBToI420 ? libyuv::ABGRToI420
                                                      : libyuv::ARGBToI420);
  }

  // TODO(jie.a.chen@intel.com): Use GPU to do the conversion.
  // Full copy #2
  int error =
      libyuv_convert_to_i420(pixel_data.data(), source->width() * 4,
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
  gfx::ColorSpace gfx_color_space(*sk_color_space);
  // 'libyuv_convert_to_i420' assumes SMPTE170M.
  // Refer to the func below to check the actual conversion:
  // third_party/libyuv/source/row_common.cc -- RGBToY(...)
  gfx_color_space = gfx_color_space.GetWithMatrixAndRange(
      gfx::ColorSpace::MatrixID::SMPTE170M, gfx::ColorSpace::RangeID::LIMITED);
  frame->set_color_space(gfx_color_space);
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

bool VideoFrame::preferAcceleratedImageBitmap() const {
  return BitmapSourceSize().Area() > kCpuEfficientFrameSize ||
         frame_->HasTextures();
}

ScriptPromise VideoFrame::CreateImageBitmap(ScriptState* script_state,
                                            base::Optional<IntRect> crop_rect,
                                            const ImageBitmapOptions* options,
                                            ExceptionState& exception_state) {
  if ((frame_->IsMappable() || frame_->HasTextures()) &&
      (frame_->format() == media::PIXEL_FORMAT_I420 ||
       (frame_->format() == media::PIXEL_FORMAT_NV12 &&
        frame_->HasTextures()))) {
    scoped_refptr<StaticBitmapImage> image;
    gfx::ColorSpace gfx_color_space = frame_->ColorSpace();
    gfx_color_space = gfx_color_space.GetWithMatrixAndRange(
        gfx::ColorSpace::MatrixID::RGB, gfx::ColorSpace::RangeID::FULL);
    auto sk_color_space = gfx_color_space.ToSkColorSpace();
    if (!sk_color_space) {
      sk_color_space = SkColorSpace::MakeSRGB();
    }

    if (!preferAcceleratedImageBitmap()) {
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

      SkImageInfo info =
          SkImageInfo::Make(visibleWidth(), visibleHeight(), kN32_SkColorType,
                            kUnpremul_SkAlphaType, std::move(sk_color_space));
      sk_sp<SkImage> skImage =
          SkImage::MakeRasterData(info, image_pixels, bytes_per_row);
      image = UnacceleratedStaticBitmapImage::Create(std::move(skImage));
    } else {
      viz::RasterContextProvider* raster_context_provider =
          Platform::Current()->SharedMainThreadContextProvider();
      gpu::SharedImageInterface* shared_image_interface =
          raster_context_provider->SharedImageInterface();
      uint32_t usage = gpu::SHARED_IMAGE_USAGE_GLES2;
      if (raster_context_provider->ContextCapabilities().supports_oop_raster) {
        usage |= gpu::SHARED_IMAGE_USAGE_RASTER |
                 gpu::SHARED_IMAGE_USAGE_OOP_RASTERIZATION;
      }

      gpu::MailboxHolder dest_holder;
      // Use coded_size() to comply with media::ConvertFromVideoFrameYUV.
      dest_holder.mailbox = shared_image_interface->CreateSharedImage(
          viz::ResourceFormat::RGBA_8888, frame_->coded_size(),
          gfx::ColorSpace(), usage);
      dest_holder.sync_token = shared_image_interface->GenUnverifiedSyncToken();
      dest_holder.texture_target = GL_TEXTURE_2D;

      media::ConvertFromVideoFrameYUV(frame_.get(), raster_context_provider,
                                      dest_holder);
      gpu::SyncToken sync_token;
      raster_context_provider->RasterInterface()
          ->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());

      auto release_callback = viz::SingleReleaseCallback::Create(base::BindOnce(
          [](gpu::SharedImageInterface* sii, gpu::Mailbox mailbox,
             const gpu::SyncToken& sync_token, bool is_lost) {
            // Ideally the SharedImage could be release here this way:
            //   sii->DestroySharedImage(sync_token, mailbox);
            // But AcceleratedStaticBitmapImage leaks it when
            // PaintImageForCurrentFrame() is called by ImageBitmap. So the
            // 'sync_token' is not precise to destroy the mailbox.
          },
          base::Unretained(shared_image_interface), dest_holder.mailbox));

      const SkImageInfo sk_image_info =
          SkImageInfo::Make(codedWidth(), codedHeight(), kN32_SkColorType,
                            kUnpremul_SkAlphaType, std::move(sk_color_space));

      image = AcceleratedStaticBitmapImage::CreateFromCanvasMailbox(
          dest_holder.mailbox, sync_token, 0u, sk_image_info,
          dest_holder.texture_target, true,
          SharedGpuContext::ContextProviderWrapper(),
          base::PlatformThread::CurrentRef(),
          Thread::Current()->GetTaskRunner(), std::move(release_callback));
    }

    ImageBitmap* image_bitmap =
        MakeGarbageCollected<ImageBitmap>(image, crop_rect, options);
    return ImageBitmapSource::FulfillImageBitmap(script_state, image_bitmap,
                                                 exception_state);
  }

  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Unsupported VideoFrame.");
  return ScriptPromise();
}

}  // namespace blink
