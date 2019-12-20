// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vp8_vaapi_video_decoder_delegate.h"

#include "media/gpu/decode_surface_handler.h"
#include "media/gpu/vaapi/va_surface.h"
#include "media/gpu/vaapi/vaapi_common.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"

namespace media {

VP8VaapiVideoDecoderDelegate::VP8VaapiVideoDecoderDelegate(
    DecodeSurfaceHandler<VASurface>* const vaapi_dec,
    scoped_refptr<VaapiWrapper> vaapi_wrapper)
    : VaapiVideoDecoderDelegate(vaapi_dec, std::move(vaapi_wrapper)) {}

VP8VaapiVideoDecoderDelegate::~VP8VaapiVideoDecoderDelegate() = default;

scoped_refptr<VP8Picture> VP8VaapiVideoDecoderDelegate::CreateVP8Picture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const auto va_surface = vaapi_dec_->CreateSurface();
  if (!va_surface)
    return nullptr;

  return new VaapiVP8Picture(std::move(va_surface));
}

bool VP8VaapiVideoDecoderDelegate::SubmitDecode(
    scoped_refptr<VP8Picture> pic,
    const Vp8ReferenceFrameVector& reference_frames) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto va_surface_id = pic->AsVaapiVP8Picture()->va_surface()->id();

  if (!FillVP8DataStructures(vaapi_wrapper_.get(), va_surface_id,
                             *pic->frame_hdr, reference_frames)) {
    return false;
  }

  return vaapi_wrapper_->ExecuteAndDestroyPendingBuffers(va_surface_id);
}

bool VP8VaapiVideoDecoderDelegate::OutputPicture(
    scoped_refptr<VP8Picture> pic) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const VaapiVP8Picture* vaapi_pic = pic->AsVaapiVP8Picture();
  vaapi_dec_->SurfaceReady(vaapi_pic->va_surface(), vaapi_pic->bitstream_id(),
                           vaapi_pic->visible_rect(),
                           vaapi_pic->get_colorspace());
  return true;
}

}  // namespace media
