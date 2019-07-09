// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CORE_PREVIEWS_CONSTANTS_H_
#define COMPONENTS_PREVIEWS_CORE_PREVIEWS_CONSTANTS_H_

namespace previews {

// The local histogram used by PreviewsOptimizationGuide to record the result of
// UpdateHints().
extern const char kPreviewsOptimizationGuideUpdateHintsResultHistogramString[];

// The local histogram used by PreviewsOptimizationGuide to record that a hint
// finished loading.
extern const char kPreviewsOptimizationGuideOnLoadedHintResultHistogramString[];

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CORE_PREVIEWS_CONSTANTS_H_
