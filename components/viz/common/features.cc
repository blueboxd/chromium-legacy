// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/features.h"

#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "components/viz/common/switches.h"

#if defined(OS_ANDROID)
#include "gpu/config/gpu_finch_features.h"  // nogncheck
#endif

namespace features {

constexpr char kProvider[] = "provider";
constexpr char kDrawQuad[] = "draw_quad";
constexpr char kSurfaceLayer[] = "surface_layer";

const base::Feature kEnableSurfaceSynchronization{
    "SurfaceSynchronization", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables running the display compositor as part of the viz service in the GPU
// process. This is also referred to as out-of-process display compositor
// (OOP-D).
// TODO(dnicoara): Look at enabling Chromecast support when ChromeOS support is
// ready.
#if defined(OS_CHROMEOS) || defined(IS_CHROMECAST)
const base::Feature kVizDisplayCompositor{"VizDisplayCompositor",
                                          base::FEATURE_DISABLED_BY_DEFAULT};
#else
const base::Feature kVizDisplayCompositor{"VizDisplayCompositor",
                                          base::FEATURE_ENABLED_BY_DEFAULT};
#endif

// Enables running the Viz-assisted hit-test logic. We still need to keep the
// VizHitTestDrawQuad and VizHitTestSurfaceLayer features for finch launch.
const base::Feature kEnableVizHitTestDrawQuad{"VizHitTestDrawQuad",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kEnableVizHitTestSurfaceLayer{
    "VizHitTestSurfaceLayer", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableVizHitTest{"VizHitTest",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

// Use Skia's readback API instead of GLRendererCopier.
const base::Feature kUseSkiaForGLReadback{"UseSkiaForGLReadback",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Use the SkiaRenderer.
const base::Feature kUseSkiaRenderer{"UseSkiaRenderer",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Use the SkiaRenderer without DDL.
const base::Feature kUseSkiaRendererNonDDL{"UseSkiaRendererNonDDL",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// Use the SkiaRenderer to record SkPicture.
const base::Feature kRecordSkPicture{"RecordSkPicture",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

bool IsSurfaceSynchronizationEnabled() {
  return IsVizDisplayCompositorEnabled() ||
         base::FeatureList::IsEnabled(kEnableSurfaceSynchronization);
}

bool IsVizDisplayCompositorEnabled() {
#if defined(OS_MACOSX) || defined(OS_WIN)
  // We can't remove the feature switch yet because OOP-D isn't enabled on all
  // platforms but turning it off on Mac and Windows isn't supported. Don't
  // check the feature switch for these platforms anymore.
  return true;
#else
#if defined(OS_ANDROID)
  if (features::IsAndroidSurfaceControlEnabled())
    return true;
#endif
  return base::FeatureList::IsEnabled(kVizDisplayCompositor);
#endif
}

bool IsVizHitTestingDebugEnabled() {
  return features::IsVizHitTestingEnabled() &&
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kEnableVizHitTestDebug);
}

// VizHitTest is considered enabled when any of its variant is turned on, or
// when VizDisplayCompositor is turned on.
bool IsVizHitTestingEnabled() {
  return base::FeatureList::IsEnabled(features::kEnableVizHitTest) ||
         base::FeatureList::IsEnabled(kVizDisplayCompositor);
}

// VizHitTestDrawQuad is enabled when this feature is explicitly enabled on
// chrome://flags, or when VizHitTest is enabled but VizHitTestSurfaceLayer is
// turned off.
bool IsVizHitTestingDrawQuadEnabled() {
  return GetFieldTrialParamValueByFeature(features::kEnableVizHitTest,
                                          kProvider) == kDrawQuad ||
         (IsVizHitTestingEnabled() && !IsVizHitTestingSurfaceLayerEnabled());
}

// VizHitTestSurfaceLayer is enabled when this feature is explicitly enabled on
// chrome://flags, or when it is enabled by finch and chrome://flags does not
// conflict.
bool IsVizHitTestingSurfaceLayerEnabled() {
  return GetFieldTrialParamValueByFeature(features::kEnableVizHitTest,
                                          kProvider) == kSurfaceLayer ||
         (IsVizHitTestingEnabled() &&
          GetFieldTrialParamValueByFeature(features::kEnableVizHitTest,
                                           kProvider) != kDrawQuad &&
          base::FeatureList::IsEnabled(kEnableVizHitTestSurfaceLayer));
}

bool IsUsingSkiaForGLReadback() {
  return base::FeatureList::IsEnabled(kUseSkiaForGLReadback);
}

bool IsUsingSkiaRenderer() {
  // We require OOP-D everywhere but WebView.
  bool enabled = base::FeatureList::IsEnabled(kUseSkiaRenderer);
#if !defined(OS_ANDROID)
  if (enabled && !IsVizDisplayCompositorEnabled()) {
    DLOG(ERROR) << "UseSkiaRenderer requires VizDisplayCompositor.";
    return false;
  }
#endif  // !defined(OS_ANDROID)
  return enabled;
}

bool IsUsingSkiaRendererNonDDL() {
  // We require OOP-D everywhere but WebView.
  bool enabled = base::FeatureList::IsEnabled(kUseSkiaRendererNonDDL);
#if !defined(OS_ANDROID)
  if (enabled && !IsVizDisplayCompositorEnabled()) {
    DLOG(ERROR) << "UseSkiaRendererNonDDL requires VizDisplayCompositor.";
    return false;
  }
#endif  // !defined(OS_ANDROID)
  return enabled;
}

bool IsRecordingSkPicture() {
  return IsUsingSkiaRenderer() &&
         base::FeatureList::IsEnabled(kRecordSkPicture);
}

}  // namespace features
