// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_image_decoder.h"

#include <va/va.h>

#include "base/logging.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"

namespace media {

namespace {

VAProfile ConvertToVAProfile(VaapiImageDecoder::Type type) {
  switch (type) {
    case VaapiImageDecoder::Type::kJpeg:
      return VAProfileJPEGBaseline;
    case VaapiImageDecoder::Type::kWebP:
      return VAProfileVP8Version0_3;
    default:
      NOTREACHED() << "Undefined Type value";
      return VAProfileNone;
  }
}

}  // namespace

VaapiImageDecoder::VaapiImageDecoder()
    : va_surface_id_(VA_INVALID_SURFACE), va_rt_format_(kInvalidVaRtFormat) {}

VaapiImageDecoder::~VaapiImageDecoder() = default;

bool VaapiImageDecoder::Initialize(const base::RepeatingClosure& error_uma_cb) {
  const VAProfile va_profile = ConvertToVAProfile(GetType());
  vaapi_wrapper_ =
      VaapiWrapper::Create(VaapiWrapper::kDecode, va_profile, error_uma_cb);
  return !!vaapi_wrapper_;
}

}  // namespace media
