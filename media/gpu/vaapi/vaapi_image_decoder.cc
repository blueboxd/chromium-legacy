// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_image_decoder.h"

#include "media/gpu/vaapi/vaapi_wrapper.h"

namespace media {

VaapiImageDecoder::VaapiImageDecoder(VAProfile va_profile)
    : va_profile_(va_profile),
      va_surface_id_(VA_INVALID_SURFACE),
      va_rt_format_(kInvalidVaRtFormat) {}

VaapiImageDecoder::~VaapiImageDecoder() = default;

bool VaapiImageDecoder::Initialize(const base::RepeatingClosure& error_uma_cb) {
  vaapi_wrapper_ =
      VaapiWrapper::Create(VaapiWrapper::kDecode, va_profile_, error_uma_cb);
  return !!vaapi_wrapper_;
}

}  // namespace media
