// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_IMAGE_DECODER_H_
#define MEDIA_GPU_VAAPI_VAAPI_IMAGE_DECODER_H_

#include <stdint.h>

#include <va/va.h>

#include "base/callback_forward.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/config/gpu_info.h"

namespace media {

class VASurface;
class VaapiWrapper;

enum class VaapiImageDecodeStatus : uint32_t {
  kSuccess,
  kParseFailed,
  kUnsupportedImage,
  kUnsupportedSubsampling,
  kSurfaceCreationFailed,
  kSubmitVABuffersFailed,
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
  virtual ~VaapiImageDecoder();

  // Initializes |vaapi_wrapper_| in kDecode mode with the
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

  // Returns the type of image supported by this decoder.
  virtual gpu::ImageDecodeAcceleratorType GetType() const = 0;

  // Returns the image profile supported by this decoder.
  gpu::ImageDecodeAcceleratorSupportedProfile GetSupportedProfile() const;

 protected:
  explicit VaapiImageDecoder(VAProfile va_profile);

  scoped_refptr<VaapiWrapper> vaapi_wrapper_;

 private:
  // The VA profile used for the current image decoder.
  const VAProfile va_profile_;

  DISALLOW_COPY_AND_ASSIGN(VaapiImageDecoder);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_IMAGE_DECODER_H_
