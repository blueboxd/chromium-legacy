// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_FEATURES_H_
#define COMPONENTS_VIZ_COMMON_FEATURES_H_

#include "components/viz/common/viz_common_export.h"

#include "base/feature_list.h"

namespace features {

VIZ_COMMON_EXPORT extern const base::Feature kUseSkiaForGLReadback;
VIZ_COMMON_EXPORT extern const base::Feature kUseSkiaRenderer;
VIZ_COMMON_EXPORT extern const base::Feature kRecordSkPicture;
VIZ_COMMON_EXPORT extern const base::Feature kDisableDeJelly;
VIZ_COMMON_EXPORT extern const base::Feature kVizForWebView;
VIZ_COMMON_EXPORT extern const base::Feature kUsePreferredIntervalForVideo;
VIZ_COMMON_EXPORT extern const base::Feature kUseRealBuffersForPageFlipTest;

VIZ_COMMON_EXPORT bool IsVizHitTestingDebugEnabled();
VIZ_COMMON_EXPORT bool IsUsingSkiaForGLReadback();
VIZ_COMMON_EXPORT bool IsUsingSkiaRenderer();
VIZ_COMMON_EXPORT bool IsRecordingSkPicture();
VIZ_COMMON_EXPORT bool IsUsingVizForWebView();
VIZ_COMMON_EXPORT bool IsUsingPreferredIntervalForVideo();
VIZ_COMMON_EXPORT bool ShouldUseRealBuffersForPageFlipTest();

}  // namespace features

#endif  // COMPONENTS_VIZ_COMMON_FEATURES_H_
