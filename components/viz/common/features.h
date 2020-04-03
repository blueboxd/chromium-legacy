// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_FEATURES_H_
#define COMPONENTS_VIZ_COMMON_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/viz/common/viz_common_export.h"


namespace features {

VIZ_COMMON_EXPORT extern const base::Feature kUseSkiaForGLReadback;
VIZ_COMMON_EXPORT extern const base::Feature kUseSkiaRenderer;
VIZ_COMMON_EXPORT extern const base::Feature kRecordSkPicture;
VIZ_COMMON_EXPORT extern const base::Feature kDisableDeJelly;
#if defined(OS_ANDROID)
VIZ_COMMON_EXPORT extern const base::Feature kDynamicColorGamut;
#endif
VIZ_COMMON_EXPORT extern const base::Feature kVizForWebView;
VIZ_COMMON_EXPORT extern const base::Feature kVizFrameSubmissionForWebView;
VIZ_COMMON_EXPORT extern const base::Feature kUsePreferredIntervalForVideo;
VIZ_COMMON_EXPORT extern const base::Feature kUseRealBuffersForPageFlipTest;
VIZ_COMMON_EXPORT extern const base::Feature kSplitPartiallyOccludedQuads;
VIZ_COMMON_EXPORT extern const base::Feature kWebRtcLogCapturePipeline;

VIZ_COMMON_EXPORT bool IsVizHitTestingDebugEnabled();
VIZ_COMMON_EXPORT bool IsUsingSkiaForGLReadback();
VIZ_COMMON_EXPORT bool IsUsingSkiaRenderer();
VIZ_COMMON_EXPORT bool IsRecordingSkPicture();
#if defined(OS_ANDROID)
VIZ_COMMON_EXPORT bool IsDynamicColorGamutEnabled();
#endif
VIZ_COMMON_EXPORT bool IsUsingVizForWebView();
VIZ_COMMON_EXPORT bool IsUsingVizFrameSubmissionForWebView();
VIZ_COMMON_EXPORT bool IsUsingPreferredIntervalForVideo();
VIZ_COMMON_EXPORT bool ShouldUseRealBuffersForPageFlipTest();
VIZ_COMMON_EXPORT bool ShouldSplitPartiallyOccludedQuads();
VIZ_COMMON_EXPORT bool ShouldWebRtcLogCapturePipeline();

}  // namespace features

#endif  // COMPONENTS_VIZ_COMMON_FEATURES_H_
