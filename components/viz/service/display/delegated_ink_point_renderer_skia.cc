// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/delegated_ink_point_renderer_skia.h"

#include "components/viz/common/delegated_ink_metadata.h"

namespace viz {

void DelegatedInkPointRendererSkia::DrawDelegatedInkTrailInternal() {
  // First, filter the delegated ink points so that only ones that have a
  // timestamp that is equal to or later than the metadata still exist.
  FilterPoints();

  if (points_.size() == 0)
    return;

  // Prediction will occur here. The CL to move prediction to ui/base must land
  // first in order for this to happen.

  // If there is only one point total between |points_| and predicted points,
  // then it will match the metadata point and therefore doesn't need to be
  // drawn in this way, as it will be rendered normally.
  // TODO(1052145): Early out here if the above condition is met.

  // TODO(1052145): Draw the all remaining points in |points_| with bezier
  // curves between them onto the skia canvas.
}

}  // namespace viz
