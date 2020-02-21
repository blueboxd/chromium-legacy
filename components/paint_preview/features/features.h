// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_FEATURES_FEATURES_H_
#define COMPONENTS_PAINT_PREVIEW_FEATURES_FEATURES_H_

#include "base/feature_list.h"

namespace paint_preview {

// Used to enable the paint preview capture experiment on Android. If enabled,
// paint preview capture will be triggered for a fraction of page loads, with
// accordance to a probability threshold that is set by a field trial param.
// Metrics for the capture are logged and the resulting paint preview is then
// deleted.
extern const base::Feature kPaintPreviewCaptureExperiment;

// Used to enable a main menu item on Android that captures a paint preview for
// the current page. Metrics for the capture are logged. The resulting paint
// preview is then deleted. This intended to test whether capturing works on a
// specific site.
extern const base::Feature kPaintPreviewTest;

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_FEATURES_FEATURES_H_
