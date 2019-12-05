// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_candidate_validator.h"

#include "build/build_config.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/service/display/output_surface.h"
#include "ui/gfx/geometry/rect_conversions.h"

#if defined(OS_MACOSX)
#include "components/viz/service/display_embedder/overlay_candidate_validator_mac.h"
#endif

#if defined(OS_WIN)
#include "base/feature_list.h"
#include "components/viz/service/display_embedder/overlay_candidate_validator_win.h"
#include "gpu/config/gpu_finch_features.h"
#endif

namespace viz {
namespace {

#if defined(OS_WIN)
std::unique_ptr<OverlayCandidateValidatorWin>
CreateOverlayCandidateValidatorWin(const OutputSurface::Capabilities& caps) {
  if (caps.supports_dc_layers)
    return std::make_unique<OverlayCandidateValidatorWin>();

  return nullptr;
}
#endif

}  // namespace

std::unique_ptr<OverlayCandidateValidator> OverlayCandidateValidator::Create(
    gpu::SurfaceHandle surface_handle,
    const OutputSurface::Capabilities& capabilities,
    const RendererSettings& renderer_settings) {
  // Do not support overlay for offscreen. WebView will not get overlay support
  // due to this check as well.
  if (surface_handle == gpu::kNullSurfaceHandle)
    return nullptr;

  if (capabilities.supports_surfaceless) {
#if defined(OS_MACOSX)
    return std::make_unique<OverlayCandidateValidatorMac>(
        !renderer_settings.allow_overlays);
#endif
  } else {
#if defined(OS_WIN)
    return CreateOverlayCandidateValidatorWin(capabilities);
#endif
  }
  NOTREACHED();
  return nullptr;
}

bool OverlayCandidateValidator::StrategyNeedsOutputSurfacePlaneRemoved() {
  return false;
}

OverlayCandidateValidator::OverlayCandidateValidator() = default;
OverlayCandidateValidator::~OverlayCandidateValidator() = default;

}  // namespace viz
