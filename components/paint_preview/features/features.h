// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_FEATURES_FEATURES_H_
#define COMPONENTS_PAINT_PREVIEW_FEATURES_FEATURES_H_

#include "base/feature_list.h"

namespace paint_preview {

// Used to enable a main menu item on Android that captures a paint preview for
// the current page. Metrics for the capture are logged and a toast is raised.
// The resulting paint preview is then deleted. This intended to test whether
// capturing works on a specific site.
extern const base::Feature kPaintPreviewTest;

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_FEATURES_FEATURES_H_
