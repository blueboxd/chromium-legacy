// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METAL_UTIL_HDR_COPIER_LAYER_H_
#define COMPONENTS_METAL_UTIL_HDR_COPIER_LAYER_H_

#include "components/metal_util/metal_util_export.h"

@class CALayer;

namespace metal {

// Create a layer which may have its contents set an HDR IOSurface via
// the -[CALayer setContents:] method.
// - The IOSurface specified to setContents must have pixel format
//   kCVPixelFormatType_64RGBAHalf or kCVPixelFormatType_ARGB2101010LEPacked,
//   any other pixel formats will be NOTREACHED.
// - This layer will, in setContents, blit the contents of the specified
//   IOSurface to an HDR-capable CAMetalLayer.
CALayer* METAL_UTIL_EXPORT CreateHDRCopierLayer();

}  // namespace metal

#endif  // COMPONENTS_METAL_UTIL_HDR_COPIER_LAYER_H_
