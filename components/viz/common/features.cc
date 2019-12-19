// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/features.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "components/viz/common/switches.h"
#include "gpu/config/gpu_finch_features.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace features {

// Enables running the display compositor as part of the viz service in the GPU
// process. This is also referred to as out-of-process display compositor
// (OOP-D).
const base::Feature kVizDisplayCompositor{"VizDisplayCompositor",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Use Skia's readback API instead of GLRendererCopier.
const base::Feature kUseSkiaForGLReadback{"UseSkiaForGLReadback",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// Use the SkiaRenderer.
#if defined(OS_LINUX) && !(defined(OS_CHROMEOS) || BUILDFLAG(IS_CHROMECAST))
const base::Feature kUseSkiaRenderer{"UseSkiaRenderer",
                                     base::FEATURE_ENABLED_BY_DEFAULT};
#else
const base::Feature kUseSkiaRenderer{"UseSkiaRenderer",
                                     base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// Use the SkiaRenderer to record SkPicture.
const base::Feature kRecordSkPicture{"RecordSkPicture",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// Kill-switch to disable de-jelly, even if flags/properties indicate it should
// be enabled.
const base::Feature kDisableDeJelly{"DisableDeJelly",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// Viz for WebView architecture.
const base::Feature kVizForWebView{"VizForWebView",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

bool IsVizDisplayCompositorEnabled() {
  // VizDisplayCompositor is always enabled except for WebView. Since we can't
  // differentiate between Android browser and WebView at compile time we still
  // need to check the feature there.
  // TODO(kylechar): Switch over any remaining places this is needed in WebView
  // to check VizForWebView feature instead of VizDisplayCompositor.
#if defined(OS_ANDROID)
  return base::FeatureList::IsEnabled(kVizDisplayCompositor);
#else
  return true;
#endif
}

const base::Feature kUsePreferredIntervalForVideo{
    "UsePreferredIntervalForVideo", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsVizHitTestingDebugEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableVizHitTestDebug);
}

bool IsUsingSkiaForGLReadback() {
  return base::FeatureList::IsEnabled(kUseSkiaForGLReadback);
}

bool IsUsingSkiaRenderer() {
#if defined(OS_ANDROID)
  // We don't support KitKat. Check for it before looking at the feature flag
  // so that KitKat doesn't show up in Control or Enabled experiment group.
  if (base::android::BuildInfo::GetInstance()->sdk_int() <=
      base::android::SDK_VERSION_KITKAT)
    return false;
#endif

  return base::FeatureList::IsEnabled(kUseSkiaRenderer) ||
         base::FeatureList::IsEnabled(kVulkan);
}

bool IsRecordingSkPicture() {
  return IsUsingSkiaRenderer() &&
         base::FeatureList::IsEnabled(kRecordSkPicture);
}

bool IsUsingVizForWebView() {
  if (base::FeatureList::IsEnabled(kVizForWebView)) {
    DCHECK(IsVizDisplayCompositorEnabled())
        << "Enabling VizForWebView requires VizDisplayCompositor";
    return true;
  }
  return false;
}

bool IsUsingPreferredIntervalForVideo() {
  return base::FeatureList::IsEnabled(kUsePreferredIntervalForVideo);
}

}  // namespace features
