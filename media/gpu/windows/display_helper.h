// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_DISPLAY_HELPER_H_
#define MEDIA_GPU_WINDOWS_DISPLAY_HELPER_H_

#include "base/optional.h"
#include "media/base/hdr_metadata.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d11_com_defs.h"

namespace media {

// This is a very hacky way to get the display characteristics.
// It should be replaced by something that actually knows which
// display is going to be used for, well, display.
class MEDIA_GPU_EXPORT DisplayHelper {
 public:
  DisplayHelper(const ComD3D11Device& d3d11_device);
  ~DisplayHelper();

  // Return the metadata for the display, if available.  Must call
  // CacheDisplayMetadata first.
  base::Optional<DXGI_HDR_METADATA_HDR10> GetDisplayMetadata();

  // Convert |hdr_metadata| to DXGI's metadata format.
  static DXGI_HDR_METADATA_HDR10 HdrMetadataToDXGI(
      const HDRMetadata& hdr_metadata);

 private:
  void CacheDisplayMetadata(const ComD3D11Device& d3d11_device);

  base::Optional<DXGI_HDR_METADATA_HDR10> hdr_metadata_;

  DISALLOW_COPY_AND_ASSIGN(DisplayHelper);
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_DISPLAY_HELPER_H_