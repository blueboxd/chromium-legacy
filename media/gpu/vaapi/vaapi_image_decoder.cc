// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_image_decoder.h"

#include "base/logging.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"

namespace media {

VaapiImageDecoder::VaapiImageDecoder(VAProfile va_profile)
    : va_profile_(va_profile) {}

VaapiImageDecoder::~VaapiImageDecoder() = default;

bool VaapiImageDecoder::Initialize(const base::RepeatingClosure& error_uma_cb) {
  vaapi_wrapper_ =
      VaapiWrapper::Create(VaapiWrapper::kDecode, va_profile_, error_uma_cb);
  return !!vaapi_wrapper_;
}

gpu::ImageDecodeAcceleratorSupportedProfile
VaapiImageDecoder::GetSupportedProfile() const {
  if (!vaapi_wrapper_) {
    DVLOGF(1) << "The VAAPI has not been initialized";
    return gpu::ImageDecodeAcceleratorSupportedProfile();
  }

  gpu::ImageDecodeAcceleratorSupportedProfile profile;
  profile.image_type = GetType();
  DCHECK_NE(gpu::ImageDecodeAcceleratorType::kUnknown, profile.image_type);

  // Note that since |vaapi_wrapper_| was created successfully, we expect the
  // following calls to be successful. Hence the DCHECKs.
  const bool got_min_resolution = VaapiWrapper::GetDecodeMinResolution(
      va_profile_, &profile.min_encoded_dimensions);
  DCHECK(got_min_resolution);
  const bool got_max_resolution = VaapiWrapper::GetDecodeMaxResolution(
      va_profile_, &profile.max_encoded_dimensions);
  DCHECK(got_max_resolution);

  // TODO(andrescj): Ideally, we would advertise support for all the formats
  // supported by the driver. However, for now, we will only support exposing
  // YUV 4:2:0 surfaces as DmaBufs.
  DCHECK(VaapiWrapper::GetDecodeSupportedInternalFormats(va_profile_).yuv420);
  profile.subsamplings.push_back(gpu::ImageDecodeAcceleratorSubsampling::k420);
  return profile;
}

scoped_refptr<gfx::NativePixmapDmaBuf>
VaapiImageDecoder::ExportAsNativePixmapDmaBuf(VaapiImageDecodeStatus* status) {
  DCHECK(status);

  // We need to take ownership of the VASurface so that the next decode doesn't
  // attempt to use the same VASurface. Otherwise, it could overwrite the result
  // of the current decode before it's used by the caller. This VASurface will
  // self-clean at the end of this scope, but the underlying buffer should stay
  // alive because of the exported FDs.
  scoped_refptr<VASurface> va_surface = ReleaseVASurface();
  if (!va_surface) {
    DVLOGF(1) << "No decoded image available";
    *status = VaapiImageDecodeStatus::kInvalidState;
    return nullptr;
  }

  DCHECK_NE(VA_INVALID_ID, va_surface->id());
  scoped_refptr<gfx::NativePixmapDmaBuf> pixmap =
      vaapi_wrapper_->ExportVASurfaceAsNativePixmapDmaBuf(va_surface->id());
  if (!pixmap) {
    *status = VaapiImageDecodeStatus::kCannotExportSurface;
    return nullptr;
  }

  // In Intel's iHD driver the size requested for the surface may be different
  // than the buffer size of the NativePixmap because of additional alignment.
  // See https://git.io/fj6nA.
  DCHECK_LE(va_surface->size().width(), pixmap->GetBufferSize().width());
  DCHECK_LE(va_surface->size().height(), pixmap->GetBufferSize().height());

  *status = VaapiImageDecodeStatus::kSuccess;
  return pixmap;
}

}  // namespace media
