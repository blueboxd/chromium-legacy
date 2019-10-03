// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/thumbnails/thumbnail_utils.h"

#include <algorithm>

#include "components/history/core/common/thumbnail_score.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace thumbnails {

bool IsGoodClipping(ClipResult clip_result) {
  return clip_result == ClipResult::kSourceWiderThanTall ||
         clip_result == ClipResult::kSourceTallerThanWide ||
         clip_result == ClipResult::kSourceNotClipped;
}

CanvasCopyInfo GetCanvasCopyInfo(const gfx::Size& source_size,
                                 float scale_factor,
                                 const gfx::Size& unscaled_target_size) {
  DCHECK(!source_size.IsEmpty());
  DCHECK(!unscaled_target_size.IsEmpty());
  DCHECK_GT(scale_factor, 0.0f);

  const float desired_aspect =
      float{unscaled_target_size.width()} / unscaled_target_size.height();

  CanvasCopyInfo copy_info;
  copy_info.target_size =
      gfx::ScaleToFlooredSize(unscaled_target_size, scale_factor);

  // Get the clipping rect so that we can preserve the aspect ratio while
  // filling the destination.
  if (source_size.width() < unscaled_target_size.width() ||
      source_size.height() < unscaled_target_size.height()) {
    // Source image is smaller: we clip the part of source image within the
    // dest rect, and then stretch it to fill the dest rect. We don't respect
    // the aspect ratio in this case.
    gfx::Size copy_size = source_size;
    copy_size.SetToMin(copy_info.target_size);

    copy_info.copy_rect = gfx::Rect(copy_size);
    copy_info.clip_result = ClipResult::kSourceSmallerThanTarget;

  } else {
    const float src_aspect =
        float{source_size.width()} / float{source_size.height()};

    if (src_aspect > desired_aspect) {
      // Wider than tall, clip horizontally: we center the smaller
      // thumbnail in the wider screen.
      const int new_width = source_size.height() * desired_aspect;
      const int x_offset = (source_size.width() - new_width) / 2;
      copy_info.clip_result =
          (src_aspect >= history::ThumbnailScore::kTooWideAspectRatio)
              ? ClipResult::kSourceMuchWiderThanTall
              : ClipResult::kSourceWiderThanTall;
      copy_info.copy_rect.SetRect(x_offset, 0, new_width, source_size.height());

    } else if (src_aspect < desired_aspect) {
      copy_info.clip_result = ClipResult::kSourceTallerThanWide;
      copy_info.copy_rect =
          gfx::Rect(source_size.width(), source_size.width() / desired_aspect);

    } else {
      copy_info.clip_result = ClipResult::kSourceNotClipped;
      copy_info.copy_rect = gfx::Rect(source_size);
    }
  }

  return copy_info;
}

}  // namespace thumbnails
