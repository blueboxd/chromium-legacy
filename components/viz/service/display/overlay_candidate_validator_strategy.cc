// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_candidate_validator_strategy.h"

#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "components/viz/common/display/renderer_settings.h"
#include "ui/gfx/geometry/rect_conversions.h"

#if defined(OS_ANDROID)
#include "components/viz/service/display_embedder/overlay_candidate_validator_android.h"
#include "components/viz/service/display_embedder/overlay_candidate_validator_surface_control.h"
#include "gpu/config/gpu_feature_info.h"
#endif

namespace viz {

namespace {
#if defined(OS_ANDROID)
std::unique_ptr<OverlayCandidateValidatorAndroid>
CreateOverlayCandidateValidatorAndroid(
    const OutputSurface::Capabilities& caps) {
  // When SurfaceControl is enabled, any resource backed by an
  // AHardwareBuffer can be marked as an overlay candidate but it requires
  // that we use a SurfaceControl backed GLSurface. If we're creating a
  // native window backed GLSurface, the overlay processing code will
  // incorrectly assume these resources can be overlaid. So we disable all
  // overlay processing for this OutputSurface.
  const bool allow_overlays = !caps.android_surface_control_feature_enabled;

  if (allow_overlays) {
    return std::make_unique<OverlayCandidateValidatorAndroid>();
  } else {
    return nullptr;
  }
}
#endif
}  // namespace

std::unique_ptr<OverlayCandidateValidatorStrategy>
OverlayCandidateValidatorStrategy::Create(
    gpu::SurfaceHandle surface_handle,
    const OutputSurface::Capabilities& capabilities,
    const RendererSettings& renderer_settings) {
  if (surface_handle == gpu::kNullSurfaceHandle)
    return nullptr;

#if defined(OS_ANDROID)
  if (capabilities.supports_surfaceless) {
    return std::make_unique<OverlayCandidateValidatorSurfaceControl>();
  } else {
    return CreateOverlayCandidateValidatorAndroid(capabilities);
  }
#else  // Default
  return nullptr;
#endif
}

gfx::Rect
OverlayCandidateValidatorStrategy::GetOverlayDamageRectForOutputSurface(
    const OverlayCandidate& candidate) const {
  return ToEnclosedRect(candidate.display_rect);
}

OverlayCandidateValidatorStrategy::OverlayCandidateValidatorStrategy() =
    default;
OverlayCandidateValidatorStrategy::~OverlayCandidateValidatorStrategy() =
    default;

}  // namespace viz
