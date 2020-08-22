// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/yuv_util.h"

#include <GLES3/gl3.h>

#include "components/viz/common/gpu/raster_context_provider.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "media/base/video_frame.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkYUVAIndex.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"

namespace media {

namespace {

enum YUVIndex : size_t {
  kYIndex = 0,
  kUIndex = 1,
  kVIndex = 2,
};
static constexpr size_t kNumNV12Planes = kUIndex + 1;
static constexpr size_t kNumYUVPlanes = kVIndex + 1;

using YUVMailboxes = std::array<gpu::MailboxHolder, kNumYUVPlanes>;
struct YUVPlaneTextureInfo {
  GrGLTextureInfo texture = {0, 0};
  bool is_shared_image = false;
};
using YUVTexturesInfo = std::array<YUVPlaneTextureInfo, kNumYUVPlanes>;

class VideoFrameYUVMailboxesHolder {
 public:
  VideoFrameYUVMailboxesHolder(const VideoFrame* video_frame,
                               viz::RasterContextProvider* provider,
                               bool import_textures)
      : provider_(provider) {
    DCHECK(video_frame);
    DCHECK(video_frame->HasTextures() || video_frame->IsMappable());
    DCHECK(video_frame->format() == PIXEL_FORMAT_I420 |
           video_frame->format() == PIXEL_FORMAT_NV12)
        << "VideoFrame has an unsupported YUV format " << video_frame->format();
    is_nv12_ = video_frame->format() == PIXEL_FORMAT_NV12;

    DCHECK(provider_);
    auto* ri = provider_->RasterInterface();
    DCHECK(ri);

    if (video_frame->HasTextures()) {
      video_frame_owns_holders_ = true;
      for (size_t plane = 0; plane < video_frame->NumTextures(); ++plane) {
        holders_[plane] = video_frame->mailbox_holder(plane);
        DCHECK(holders_[plane].texture_target == GL_TEXTURE_2D ||
               holders_[plane].texture_target == GL_TEXTURE_EXTERNAL_OES ||
               holders_[plane].texture_target == GL_TEXTURE_RECTANGLE_ARB)
            << "Unsupported texture target " << std::hex << std::showbase
            << holders_[plane].texture_target;
        ri->WaitSyncTokenCHROMIUM(holders_[plane].sync_token.GetConstData());
      }
    } else {
      DCHECK(!is_nv12_) << "NV12 CPU backed VideoFrames aren't supported.";
      video_frame_owns_holders_ = false;
      gfx::Size y_size = video_frame->coded_size();
      gfx::Size uv_size = gfx::Size(y_size.width() / 2, y_size.height() / 2);

      auto* sii = provider_->SharedImageInterface();
      DCHECK(sii);
      uint32_t mailbox_usage;
      if (provider_->ContextCapabilities().supports_oop_raster) {
        mailbox_usage = gpu::SHARED_IMAGE_USAGE_RASTER |
                        gpu::SHARED_IMAGE_USAGE_OOP_RASTERIZATION;
      } else {
        mailbox_usage = gpu::SHARED_IMAGE_USAGE_GLES2;
      }
      for (size_t plane = 0; plane < kNumYUVPlanes; ++plane) {
        gfx::Size tex_size = plane == kYIndex ? y_size : uv_size;
        holders_[plane].mailbox = sii->CreateSharedImage(
            viz::ResourceFormat::LUMINANCE_8, tex_size,
            video_frame->ColorSpace(), kTopLeft_GrSurfaceOrigin,
            kPremul_SkAlphaType, mailbox_usage, gpu::kNullSurfaceHandle);
        holders_[plane].texture_target = GL_TEXTURE_2D;
      }

      // Split up shared image creation from upload so we only have to wait on
      // one sync token.
      ri->WaitSyncTokenCHROMIUM(sii->GenUnverifiedSyncToken().GetConstData());
      for (size_t plane = 0; plane < kNumYUVPlanes; ++plane) {
        gfx::Size tex_size = plane == kYIndex ? y_size : uv_size;
        SkImageInfo info =
            SkImageInfo::Make(tex_size.width(), tex_size.height(),
                              kGray_8_SkColorType, kUnknown_SkAlphaType);
        ri->WritePixels(holders_[plane].mailbox, 0, 0, GL_TEXTURE_2D,
                        video_frame->stride(plane), info,
                        video_frame->data(plane));
      }
    }

    if (import_textures) {
      ImportTextures();
    }
  }

  ~VideoFrameYUVMailboxesHolder() {
    auto* ri = provider_->RasterInterface();
    DCHECK(ri);
    if (imported_textures_) {
      for (auto& tex_info : textures_) {
        if (!tex_info.texture.fID)
          continue;

        if (tex_info.is_shared_image)
          ri->EndSharedImageAccessDirectCHROMIUM(tex_info.texture.fID);
        ri->DeleteGpuRasterTexture(tex_info.texture.fID);
      }
    }

    // Don't destroy shared images we don't own.
    if (video_frame_owns_holders_)
      return;

    gpu::SyncToken token;
    ri->GenUnverifiedSyncTokenCHROMIUM(token.GetData());

    auto* sii = provider_->SharedImageInterface();
    DCHECK(sii);
    for (auto& mailbox_holder : holders_) {
      if (!mailbox_holder.mailbox.IsZero())
        sii->DestroySharedImage(token, mailbox_holder.mailbox);
      mailbox_holder.mailbox.SetZero();
    }
  }

  bool is_nv12() { return is_nv12_; }

  const gpu::Mailbox& mailbox(size_t plane) {
    DCHECK_LE(plane, is_nv12_ ? kNumNV12Planes : kNumYUVPlanes);
    return holders_[plane].mailbox;
  }

  const GrGLTextureInfo& texture(size_t plane) {
    DCHECK_LE(plane, is_nv12_ ? kNumNV12Planes : kNumYUVPlanes);
    DCHECK(imported_textures_);
    return textures_[plane].texture;
  }

 private:
  void ImportTextures() {
    auto* ri = provider_->RasterInterface();
    GrGLenum skia_texture_format = is_nv12_ ? GL_RGB8 : GL_LUMINANCE8_EXT;
    for (size_t plane = 0; plane < NumPlanes(); ++plane) {
      textures_[plane].texture.fID =
          ri->CreateAndConsumeForGpuRaster(holders_[plane].mailbox);
      if (holders_[plane].mailbox.IsSharedImage()) {
        textures_[plane].is_shared_image = true;
        ri->BeginSharedImageAccessDirectCHROMIUM(
            textures_[plane].texture.fID,
            GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
      }

      textures_[plane].texture.fTarget = holders_[plane].texture_target;
      textures_[plane].texture.fFormat = skia_texture_format;
    }

    imported_textures_ = true;
  }

  size_t NumPlanes() { return is_nv12_ ? kNumNV12Planes : kNumYUVPlanes; }

  viz::RasterContextProvider* provider_ = nullptr;
  bool imported_textures_ = false;
  bool video_frame_owns_holders_ = false;
  bool is_nv12_ = false;

  YUVMailboxes holders_;
  YUVTexturesInfo textures_;
};

// Some YUVA factories infer the YUVAIndices. This helper identifies the channel
// to use for single channel textures.
static SkColorChannel GetSingleChannel(const GrBackendTexture& tex) {
  switch (tex.getBackendFormat().channelMask()) {
    case kGray_SkColorChannelFlag:  // Gray can be read as any of kR, kG, kB.
    case kRed_SkColorChannelFlag:
      return SkColorChannel::kR;
    case kAlpha_SkColorChannelFlag:
      return SkColorChannel::kA;
    default:  // multiple channels in the texture. Guess kR.
      return SkColorChannel::kR;
  }
}

SkYUVColorSpace ColorSpaceToSkYUVColorSpace(
    const gfx::ColorSpace& color_space) {
  // TODO(hubbe): This should really default to rec709.
  // https://crbug.com/828599
  SkYUVColorSpace sk_color_space = kRec601_SkYUVColorSpace;
  color_space.ToSkYUVColorSpace(&sk_color_space);
  return sk_color_space;
}

SkColorType GetCompatibleSurfaceColorType(GrGLenum format) {
  switch (format) {
    case GL_RGBA8:
      return kRGBA_8888_SkColorType;
    case GL_RGB565:
      return kRGB_565_SkColorType;
    case GL_RGBA16F:
      return kRGBA_F16_SkColorType;
    case GL_RGB8:
      return kRGB_888x_SkColorType;
    case GL_RGB10_A2:
      return kRGBA_1010102_SkColorType;
    case GL_RGBA4:
      return kARGB_4444_SkColorType;
    case GL_SRGB8_ALPHA8:
      return kRGBA_8888_SkColorType;
    default:
      NOTREACHED();
      return kUnknown_SkColorType;
  }
}

GrGLenum GetSurfaceColorFormat(GrGLenum format, GrGLenum type) {
  if (format == GL_RGBA) {
    if (type == GL_UNSIGNED_BYTE)
      return GL_RGBA8;
    if (type == GL_UNSIGNED_SHORT_4_4_4_4)
      return GL_RGBA4;
  }
  if (format == GL_RGB) {
    if (type == GL_UNSIGNED_BYTE)
      return GL_RGB8;
    if (type == GL_UNSIGNED_SHORT_5_6_5)
      return GL_RGB565;
  }
  return format;
}

bool YUVGrBackendTexturesToSkSurface(GrDirectContext* gr_context,
                                     const VideoFrame* video_frame,
                                     GrBackendTexture* yuv_textures,
                                     sk_sp<SkSurface> surface,
                                     bool flip_y,
                                     bool use_visible_rect) {
  SkYUVAIndex indices[4];

  switch (video_frame->format()) {
    case PIXEL_FORMAT_NV12:
      indices[SkYUVAIndex::kY_Index] = {
          0, GetSingleChannel(yuv_textures[0])};  // the first backend texture
      indices[SkYUVAIndex::kU_Index] = {
          1, SkColorChannel::kR};  // the second backend texture
      indices[SkYUVAIndex::kV_Index] = {1, SkColorChannel::kG};
      indices[SkYUVAIndex::kA_Index] = {-1,
                                        SkColorChannel::kA};  // no alpha plane
      break;
    case PIXEL_FORMAT_I420:
      indices[SkYUVAIndex::kY_Index] = {
          0, GetSingleChannel(yuv_textures[0])};  // the first backend texture
      indices[SkYUVAIndex::kU_Index] = {
          1, GetSingleChannel(yuv_textures[1])};  // the second backend texture
      indices[SkYUVAIndex::kV_Index] = {2, GetSingleChannel(yuv_textures[2])};
      indices[SkYUVAIndex::kA_Index] = {-1,
                                        SkColorChannel::kA};  // no alpha plane
      break;
    default:
      NOTREACHED();
      return false;
  }

  auto image = SkImage::MakeFromYUVATextures(
      gr_context, ColorSpaceToSkYUVColorSpace(video_frame->ColorSpace()),
      yuv_textures, indices,
      {video_frame->coded_size().width(), video_frame->coded_size().height()},
      kTopLeft_GrSurfaceOrigin, SkColorSpace::MakeSRGB());

  if (!image) {
    return false;
  }

  if (!use_visible_rect) {
    surface->getCanvas()->drawImage(image, 0, 0);
  } else {
    // Draw the planar SkImage to the SkSurface wrapping the WebGL texture.
    // Using drawImageRect to draw visible rect from video frame to dst texture.
    const gfx::Rect& visible_rect = video_frame->visible_rect();
    const SkRect src_rect =
        SkRect::MakeXYWH(visible_rect.x(), visible_rect.y(),
                         visible_rect.width(), visible_rect.height());
    const SkRect dst_rect =
        SkRect::MakeWH(visible_rect.width(), visible_rect.height());
    surface->getCanvas()->drawImageRect(image, src_rect, dst_rect, nullptr);
  }

  surface->flushAndSubmit();
  return true;
}

bool ConvertFromVideoFrameYUVToSkSurface(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    unsigned int texture_target,
    unsigned int texture_id,
    unsigned int internal_format,
    unsigned int type,
    bool flip_y,
    bool use_visible_rect) {
  DCHECK(video_frame->HasTextures() ||
         (video_frame->IsMappable() &&
          video_frame->format() == PIXEL_FORMAT_I420));
  GrDirectContext* gr_context = raster_context_provider->GrContext();
  DCHECK(gr_context);
  // TODO: We should compare the DCHECK vs when UpdateLastImage calls this
  // function. (https://crbug.com/674185)
  DCHECK(video_frame->format() == PIXEL_FORMAT_I420 ||
         video_frame->format() == PIXEL_FORMAT_NV12);

  gfx::Size ya_tex_size = video_frame->coded_size();
  gfx::Size uv_tex_size((ya_tex_size.width() + 1) / 2,
                        (ya_tex_size.height() + 1) / 2);

  GrGLTextureInfo backend_texture{};

  VideoFrameYUVMailboxesHolder yuv_textures_info(video_frame,
                                                 raster_context_provider, true);

  GrBackendTexture yuv_textures[3] = {
      GrBackendTexture(ya_tex_size.width(), ya_tex_size.height(),
                       GrMipMapped::kNo, yuv_textures_info.texture(kYIndex)),
      GrBackendTexture(uv_tex_size.width(), uv_tex_size.height(),
                       GrMipMapped::kNo, yuv_textures_info.texture(kUIndex)),
      GrBackendTexture(uv_tex_size.width(), uv_tex_size.height(),
                       GrMipMapped::kNo, yuv_textures_info.texture(kVIndex)),
  };

  backend_texture.fID = texture_id;
  backend_texture.fTarget = texture_target;
  backend_texture.fFormat = GetSurfaceColorFormat(internal_format, type);

  // For method like texImage2D, dst texture is Ok to use visible_rect
  // but for method like create imageBitmap from video frame, it is better
  // to use coded_size to store more pixels.
  int backend_texture_width = use_visible_rect
                                  ? video_frame->visible_rect().width()
                                  : video_frame->coded_size().width();
  int backend_texture_height = use_visible_rect
                                   ? video_frame->visible_rect().height()
                                   : video_frame->coded_size().height();

  GrBackendTexture result_texture(backend_texture_width, backend_texture_height,
                                  GrMipMapped::kNo, backend_texture);

  // Wraps the result texture in a SkSurface which allows Skia to render to it
  // (creates FBO, etc)
  auto surface = SkSurface::MakeFromBackendTexture(
      gr_context, result_texture,
      flip_y ? kBottomLeft_GrSurfaceOrigin : kTopLeft_GrSurfaceOrigin, 1,
      GetCompatibleSurfaceColorType(backend_texture.fFormat),
      SkColorSpace::MakeSRGB(), nullptr);

  if (!surface) {
    return false;
  }

  return YUVGrBackendTexturesToSkSurface(gr_context, video_frame, yuv_textures,
                                         surface, flip_y, use_visible_rect);
}

void ConvertFromVideoFrameYUVWithGrContext(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    const gpu::MailboxHolder& dest_mailbox_holder,
    unsigned int internal_format,
    unsigned int type,
    bool flip_y,
    bool use_visible_rect) {
  gpu::raster::RasterInterface* ri = raster_context_provider->RasterInterface();
  DCHECK(ri);
  ri->WaitSyncTokenCHROMIUM(dest_mailbox_holder.sync_token.GetConstData());
  GLuint dest_tex_id =
      ri->CreateAndConsumeForGpuRaster(dest_mailbox_holder.mailbox);
  if (dest_mailbox_holder.mailbox.IsSharedImage()) {
    ri->BeginSharedImageAccessDirectCHROMIUM(
        dest_tex_id, GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
  }
  // Let the SkImage fall out of scope and track the result using dest_tex_id
  ConvertFromVideoFrameYUVToSkSurface(
      video_frame, raster_context_provider, dest_mailbox_holder.texture_target,
      dest_tex_id, internal_format, type, flip_y, use_visible_rect);
  if (dest_mailbox_holder.mailbox.IsSharedImage())
    ri->EndSharedImageAccessDirectCHROMIUM(dest_tex_id);
  ri->DeleteGpuRasterTexture(dest_tex_id);
}

}  // namespace

void ConvertFromVideoFrameYUV(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    const gpu::MailboxHolder& dest_mailbox_holder,
    unsigned int internal_format,
    unsigned int type,
    bool flip_y,
    bool use_visible_rect) {
  DCHECK(raster_context_provider);
  if (raster_context_provider->GrContext()) {
    ConvertFromVideoFrameYUVWithGrContext(video_frame, raster_context_provider,
                                          dest_mailbox_holder, internal_format,
                                          type, flip_y, use_visible_rect);
    return;
  }

  auto* ri = raster_context_provider->RasterInterface();
  DCHECK(ri);
  ri->WaitSyncTokenCHROMIUM(dest_mailbox_holder.sync_token.GetConstData());
  SkYUVColorSpace color_space =
      ColorSpaceToSkYUVColorSpace(video_frame->ColorSpace());

  VideoFrameYUVMailboxesHolder yuv_mailboxes(video_frame,
                                             raster_context_provider, false);

  if (yuv_mailboxes.is_nv12()) {
    ri->ConvertNV12MailboxesToRGB(dest_mailbox_holder.mailbox, color_space,
                                  yuv_mailboxes.mailbox(kYIndex),
                                  yuv_mailboxes.mailbox(kUIndex));
  } else {
    DCHECK_EQ(video_frame->NumTextures(), kNumYUVPlanes);
    ri->ConvertYUVMailboxesToRGB(dest_mailbox_holder.mailbox, color_space,
                                 yuv_mailboxes.mailbox(kYIndex),
                                 yuv_mailboxes.mailbox(kUIndex),
                                 yuv_mailboxes.mailbox(kVIndex));
  }
}

}  // namespace media
