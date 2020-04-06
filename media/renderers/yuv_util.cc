// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/yuv_util.h"

#include <GLES3/gl3.h>

#include "components/viz/common/gpu/raster_context_provider.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "media/base/video_frame.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace media {

namespace {

static constexpr size_t kNumYUVPlanes = 3;
struct YUVPlaneTextureInfo {
  GrGLTextureInfo texture = {0, 0};
  bool is_shared_image = false;
};
using YUVTexturesInfo = std::array<YUVPlaneTextureInfo, kNumYUVPlanes>;

YUVTexturesInfo GetYUVTexturesInfo(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider) {
  YUVTexturesInfo yuv_textures_info;

  gpu::raster::RasterInterface* ri = raster_context_provider->RasterInterface();
  DCHECK(ri);
  GrGLenum skia_texture_format =
      video_frame->format() == PIXEL_FORMAT_NV12 ? GL_RGB8 : GL_R8_EXT;
  for (size_t i = 0; i < video_frame->NumTextures(); ++i) {
    // Get the texture from the mailbox and wrap it in a GrTexture.
    const gpu::MailboxHolder& mailbox_holder = video_frame->mailbox_holder(i);
    DCHECK(mailbox_holder.texture_target == GL_TEXTURE_2D ||
           mailbox_holder.texture_target == GL_TEXTURE_EXTERNAL_OES ||
           mailbox_holder.texture_target == GL_TEXTURE_RECTANGLE_ARB)
        << "Unsupported texture target " << std::hex << std::showbase
        << mailbox_holder.texture_target;
    ri->WaitSyncTokenCHROMIUM(mailbox_holder.sync_token.GetConstData());
    yuv_textures_info[i].texture.fID =
        ri->CreateAndConsumeForGpuRaster(mailbox_holder.mailbox);
    if (mailbox_holder.mailbox.IsSharedImage()) {
      yuv_textures_info[i].is_shared_image = true;
      ri->BeginSharedImageAccessDirectCHROMIUM(
          yuv_textures_info[i].texture.fID,
          GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
    }

    yuv_textures_info[i].texture.fTarget = mailbox_holder.texture_target;
    yuv_textures_info[i].texture.fFormat = skia_texture_format;
  }

  return yuv_textures_info;
}

void DeleteYUVTextures(const VideoFrame* video_frame,
                       viz::RasterContextProvider* raster_context_provider,
                       const YUVTexturesInfo& yuv_textures_info) {
  gpu::raster::RasterInterface* ri = raster_context_provider->RasterInterface();
  DCHECK(ri);

  for (size_t i = 0; i < video_frame->NumTextures(); ++i) {
    if (yuv_textures_info[i].is_shared_image)
      ri->EndSharedImageAccessDirectCHROMIUM(yuv_textures_info[i].texture.fID);
    ri->DeleteGpuRasterTexture(yuv_textures_info[i].texture.fID);
  }
}

}  // namespace

void ConvertFromVideoFrameYUVTextures(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    unsigned int texture_out_target,
    unsigned int texture_out_id) {
  // Let the SkImage fall out of scope and track the result texture using
  // texture_out_id.
  NewSkImageFromVideoFrameYUVTexturesWithExternalBackend(
      video_frame, raster_context_provider, texture_out_target, texture_out_id);
}

sk_sp<SkImage> NewSkImageFromVideoFrameYUVTexturesWithExternalBackend(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    unsigned int texture_target,
    unsigned int texture_id) {
  DCHECK(video_frame->HasTextures());
  GrContext* gr_context = raster_context_provider->GrContext();
  DCHECK(gr_context);
  // TODO: We should compare the DCHECK vs when UpdateLastImage calls this
  // function. (https://crbug.com/674185)
  DCHECK(video_frame->format() == PIXEL_FORMAT_I420 ||
         video_frame->format() == PIXEL_FORMAT_NV12);

  gfx::Size ya_tex_size = video_frame->coded_size();
  gfx::Size uv_tex_size((ya_tex_size.width() + 1) / 2,
                        (ya_tex_size.height() + 1) / 2);

  GrGLTextureInfo backend_texture{};

  YUVTexturesInfo yuv_textures_info =
      GetYUVTexturesInfo(video_frame, raster_context_provider);

  GrBackendTexture yuv_textures[3] = {
      GrBackendTexture(ya_tex_size.width(), ya_tex_size.height(),
                       GrMipMapped::kNo, yuv_textures_info[0].texture),
      GrBackendTexture(uv_tex_size.width(), uv_tex_size.height(),
                       GrMipMapped::kNo, yuv_textures_info[1].texture),
      GrBackendTexture(uv_tex_size.width(), uv_tex_size.height(),
                       GrMipMapped::kNo, yuv_textures_info[2].texture),
  };
  backend_texture.fID = texture_id;
  backend_texture.fTarget = texture_target;
  backend_texture.fFormat = GL_RGBA8;
  GrBackendTexture result_texture(video_frame->coded_size().width(),
                                  video_frame->coded_size().height(),
                                  GrMipMapped::kNo, backend_texture);

  sk_sp<SkImage> img = YUVGrBackendTexturesToSkImage(
      gr_context, video_frame->ColorSpace(), video_frame->format(),
      yuv_textures, result_texture);
  gr_context->flush();

  DeleteYUVTextures(video_frame, raster_context_provider, yuv_textures_info);

  return img;
}

sk_sp<SkImage> YUVGrBackendTexturesToSkImage(
    GrContext* gr_context,
    gfx::ColorSpace video_color_space,
    VideoPixelFormat video_format,
    GrBackendTexture* yuv_textures,
    const GrBackendTexture& result_texture) {
  // TODO(hubbe): This should really default to rec709.
  // https://crbug.com/828599
  SkYUVColorSpace color_space = kRec601_SkYUVColorSpace;
  video_color_space.ToSkYUVColorSpace(&color_space);

  switch (video_format) {
    case PIXEL_FORMAT_NV12:
      return SkImage::MakeFromNV12TexturesCopyWithExternalBackend(
          gr_context, color_space, yuv_textures, kTopLeft_GrSurfaceOrigin,
          result_texture);
    case PIXEL_FORMAT_I420:
      return SkImage::MakeFromYUVTexturesCopyWithExternalBackend(
          gr_context, color_space, yuv_textures, kTopLeft_GrSurfaceOrigin,
          result_texture);
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace media
