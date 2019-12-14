// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_candidate_validator_strategy.h"

#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "components/viz/common/display/renderer_settings.h"
#include "ui/gfx/geometry/rect_conversions.h"

#if defined(OS_ANDROID)
#include "components/viz/service/display_embedder/overlay_candidate_validator_surface_control.h"
#include "gpu/config/gpu_feature_info.h"
#endif

namespace viz {
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
  }
  return nullptr;
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
