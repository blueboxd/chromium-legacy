// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <va/va.h>

#include "base/files/file_util.h"
#include "media/base/video_types.h"
#include "media/gpu/vaapi/test/macros.h"
#include "media/gpu/vaapi/test/shared_va_surface.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gfx/codec/png_codec.h"

namespace media {
namespace vaapi_test {

namespace {

// Derives the VAImage metadata and image data from |surface_id| in |display|,
// returning true on success.
bool DeriveImage(VADisplay display,
                 VASurfaceID surface_id,
                 VAImage* image,
                 uint8_t** image_data) {
  VAStatus res = vaDeriveImage(display, surface_id, image);
  VLOG_IF(2, (res != VA_STATUS_SUCCESS))
      << "vaDeriveImage failed, VA error: " << vaErrorStr(res);

  if (image->format.fourcc != VA_FOURCC_NV12) {
    VLOG(2) << "Test decoder binary does not support derived surface format "
            << "with fourcc " << media::FourccToString(image->format.fourcc);
    res = vaDestroyImage(display, image->image_id);
    VA_LOG_ASSERT(res, "vaDestroyImage");
    return false;
  }

  res = vaMapBuffer(display, image->buf, reinterpret_cast<void**>(image_data));
  VA_LOG_ASSERT(res, "vaMapBuffer");
  return true;
}

// Maps the image data from |surface_id| in |display| with given |size| by
// attempting to derive into |image| and |image_data|, or creating an NV12
// VAImage to use with vaGetImage as fallback and setting |image| and
// |image_data| accordingly.
void GetSurfaceImage(VADisplay display,
                     VASurfaceID surface_id,
                     const gfx::Size size,
                     VAImage* image,
                     uint8_t** image_data) {
  // First attempt to derive the image from the surface.
  if (DeriveImage(display, surface_id, image, image_data))
    return;

  // Fall back to getting the image as NV12.
  VAImageFormat format{
      .fourcc = VA_FOURCC_NV12,
      .byte_order = VA_LSB_FIRST,
      .bits_per_pixel = 12,
  };
  VAStatus res =
      vaCreateImage(display, &format, size.width(), size.height(), image);
  VA_LOG_ASSERT(res, "vaCreateImage");

  res = vaGetImage(display, surface_id, 0, 0, size.width(), size.height(),
                   image->image_id);
  VA_LOG_ASSERT(res, "vaGetImage");

  res = vaMapBuffer(display, image->buf, reinterpret_cast<void**>(image_data));
  VA_LOG_ASSERT(res, "vaMapBuffer");
}

}  // namespace

SharedVASurface::SharedVASurface(const VaapiDevice& va,
                                 VASurfaceID id,
                                 const gfx::Size& size,
                                 unsigned int format)
    : va_(va), id_(id), size_(size), internal_va_format_(format) {}

// static
scoped_refptr<SharedVASurface> SharedVASurface::Create(const VaapiDevice& va,
                                                       const gfx::Size& size,
                                                       VASurfaceAttrib attrib) {
  VASurfaceID surface_id;
  VAStatus res =
      vaCreateSurfaces(va.display(), va.internal_va_format(),
                       base::checked_cast<unsigned int>(size.width()),
                       base::checked_cast<unsigned int>(size.height()),
                       &surface_id, 1u, &attrib, 1u);
  VA_LOG_ASSERT(res, "vaCreateSurfaces");
  VLOG(1) << "created surface: " << surface_id;
  return base::WrapRefCounted(
      new SharedVASurface(va, surface_id, size, va.internal_va_format()));
}

SharedVASurface::~SharedVASurface() {
  VAStatus res =
      vaDestroySurfaces(va_.display(), const_cast<VASurfaceID*>(&id_), 1u);
  VA_LOG_ASSERT(res, "vaDestroySurfaces");
  VLOG(1) << "destroyed surface " << id_;
}

void SharedVASurface::SaveAsPNG(const std::string& path) {
  VAImage image;
  uint8_t* image_data;

  GetSurfaceImage(va_.display(), id_, size_, &image, &image_data);

  // Convert the NV12 image data to ARGB and write to |path|.
  const size_t argb_stride = image.width * 4;
  auto argb_data = std::make_unique<uint8_t[]>(argb_stride * image.height);
  const int convert_res = libyuv::NV12ToARGB(
      (uint8_t*)(image_data + image.offsets[0]), image.pitches[0],
      (uint8_t*)(image_data + image.offsets[1]), image.pitches[1],
      argb_data.get(), argb_stride, image.width, image.height);
  DCHECK(convert_res == 0);

  std::vector<unsigned char> image_buffer;
  const bool result = gfx::PNGCodec::Encode(
      argb_data.get(), gfx::PNGCodec::FORMAT_BGRA, size_, argb_stride,
      true /* discard_transparency */, std::vector<gfx::PNGCodec::Comment>(),
      &image_buffer);
  DCHECK(result);

  LOG_ASSERT(base::WriteFile(base::FilePath(path), image_buffer));

  // Clean up VA handles.
  VAStatus res = vaUnmapBuffer(va_.display(), image.buf);
  VA_LOG_ASSERT(res, "vaUnmapBuffer");

  res = vaDestroyImage(va_.display(), image.image_id);
  VA_LOG_ASSERT(res, "vaDestroyImage");
}

}  // namespace vaapi_test
}  // namespace media
