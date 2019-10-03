// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_UTILS_H_
#define CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_UTILS_H_

#include "base/macros.h"
#include "ui/base/resource/scale_factor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace thumbnails {

// The result of clipping. This can be used to determine if the generated
// thumbnail is good or not.
enum class ClipResult {
  kSourceNotClipped,         // Source and target aspect ratios are identical.
  kSourceSmallerThanTarget,  // Source image is smaller than target.
  kSourceMuchWiderThanTall,  // Wider than tall by 2x+, clip horizontally.
  kSourceWiderThanTall,      // Wider than tall, clip horizontally.
  kSourceTallerThanWide,     // Taller than wide, clip vertically.
};

// Describes how a thumbnail bitmap should be generated from a target surface.
struct CanvasCopyInfo {
  // How the source canvas is clipped to achieve the target size.
  ClipResult clip_result = ClipResult::kSourceNotClipped;

  // Cropping rectangle for the source canvas, in pixels (not DIPs).
  gfx::Rect copy_rect;

  // Size of the target bitmap in pixels.
  gfx::Size target_size;
};

bool IsGoodClipping(ClipResult clip_result);

// The implementation of the 'classic' thumbnail cropping algorithm. It is not
// content-driven in any meaningful way. Rather, the choice of a cropping region
// is based on relation between source and target sizes. The selected source
// region is then rescaled into the target thumbnail image.
//
// Provides information necessary to crop-and-resize image data from a source
// canvas of |source_size|. Auxiliary |scale_factor| helps compute the target
// thumbnail size to be copied from the backing store, in pixels. The return
// value contains the type of clip and the clip parameters.
CanvasCopyInfo GetCanvasCopyInfo(const gfx::Size& source_size,
                                 float scale_factor,
                                 const gfx::Size& unscaled_target_size);

}  // namespace thumbnails

#endif  // CHROME_BROWSER_UI_THUMBNAILS_THUMBNAIL_UTILS_H_
