// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DELEGATED_INK_POINT_RENDERER_SKIA_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DELEGATED_INK_POINT_RENDERER_SKIA_H_

#include "components/viz/service/display/delegated_ink_point_renderer_base.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

// This class handles drawing the delegated ink trail when the Skia renderer
// is in use by filtering everything out with timestamps before the metadata,
// predicting another point or two, and drawing the points with bezier curves
// between them with Skia commands onto the canvas provided by the Skia
// renderer, the |current_canvas_|.
// TODO(1052145): Specify exactly how many points are predicted.
//
// For more information on the feature, please see the explainer:
// https://github.com/WICG/ink-enhancement/blob/master/README.md
class VIZ_SERVICE_EXPORT DelegatedInkPointRendererSkia
    : public DelegatedInkPointRendererBase {
 public:
  DelegatedInkPointRendererSkia() = default;
  DelegatedInkPointRendererSkia(const DelegatedInkPointRendererSkia&) = delete;
  DelegatedInkPointRendererSkia& operator=(
      const DelegatedInkPointRendererSkia&) = delete;

 private:
  void DrawDelegatedInkTrailInternal() override;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DELEGATED_INK_POINT_RENDERER_SKIA_H_
