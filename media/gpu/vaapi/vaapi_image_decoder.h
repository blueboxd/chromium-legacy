// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_IMAGE_DECODER_H_
#define MEDIA_GPU_VAAPI_VAAPI_IMAGE_DECODER_H_

#include <stdint.h>

#include "base/callback_forward.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "ui/gfx/geometry/size.h"

// This data type is defined in va/va.h using typedef, reproduced here.
typedef unsigned int VASurfaceID;

namespace media {

class VASurface;
class VaapiWrapper;

constexpr unsigned int kInvalidVaRtFormat = 0u;

enum class VaapiImageDecodeStatus : uint32_t {
  kSuccess,
  kParseJpegFailed,
  kUnsupportedJpeg,
  kUnsupportedSubsampling,
  kSurfaceCreationFailed,
  kSubmitPicParamsFailed,
  kSubmitIQMatrixFailed,
  kSubmitHuffmanFailed,
  kSubmitSliceParamsFailed,
  kSubmitSliceDataFailed,
  kExecuteDecodeFailed,
  kUnsupportedSurfaceFormat,
  kCannotGetImage,
  kInvalidState,
};

// This class abstracts the idea of VA-API format-specific decoders. It is the
// responsibility of each subclass to initialize |vaapi_wrapper_| appropriately
// for the purpose of performing hardware-accelerated image decodes of a
// particular format (e.g. JPEG or WebP). Objects of this class are not
// thread-safe, but they are also not thread-affine, i.e., the caller is free to
// call the methods on any thread, but calls must be synchronized externally.
// TODO(gildekel): move more common image decoding functions to this class as
// more implementing classes are added (e.g. VaapiWebPDecoder).
class VaapiImageDecoder {
 public:
  // Type of image decoder.
  enum class Type {
    kJpeg,
    kWebP,
  };

  virtual ~VaapiImageDecoder();

  // Uses GetType() to initialize |vaapi_wrapper_| in kDecode mode with the
  // appropriate VAAPI profile and |error_uma_cb| for error reporting.
  bool Initialize(const base::RepeatingClosure& error_uma_cb);

  // Decodes a picture. It will fill VA-API parameters and call the
  // corresponding VA-API methods according to the image in |encoded_image|.
  // The image will be decoded into an internally allocated VA surface. It
  // will be returned as an unowned VASurface, which remains valid until the
  // next Decode() call or destruction of this class. Returns nullptr on
  // failure and sets *|status| to the reason for failure.
  virtual scoped_refptr<VASurface> Decode(
      base::span<const uint8_t> encoded_image,
      VaapiImageDecodeStatus* status) = 0;

  // Returns the type of the current decoder.
  virtual Type GetType() const = 0;

 protected:
  VaapiImageDecoder();

  scoped_refptr<VaapiWrapper> vaapi_wrapper_;

  // The current VA surface for decoding.
  VASurfaceID va_surface_id_;
  // The coded size associated with |va_surface_id_|.
  gfx::Size coded_size_;
  // The VA RT format associated with |va_surface_id_|.
  unsigned int va_rt_format_;

  DISALLOW_COPY_AND_ASSIGN(VaapiImageDecoder);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_IMAGE_DECODER_H_
