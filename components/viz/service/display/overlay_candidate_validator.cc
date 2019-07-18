// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_candidate_validator.h"

#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/gpu/context_provider.h"
#include "ui/gfx/geometry/rect_conversions.h"

#if defined(OS_ANDROID)
#include "components/viz/service/display_embedder/overlay_candidate_validator_android.h"
#include "components/viz/service/display_embedder/overlay_candidate_validator_surface_control.h"
#include "gpu/config/gpu_feature_info.h"
#endif

#if defined(OS_MACOSX)
#include "components/viz/service/display_embedder/overlay_candidate_validator_mac.h"
#endif

#if defined(OS_WIN)
#include "base/feature_list.h"
#include "components/viz/service/display_embedder/overlay_candidate_validator_win.h"
#include "gpu/config/gpu_finch_features.h"
#endif

#if defined(USE_OZONE)
#include "components/viz/service/display_embedder/overlay_candidate_validator_ozone.h"
#include "ui/ozone/public/overlay_manager_ozone.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace viz {
namespace {

#if defined(USE_OZONE)
std::unique_ptr<OverlayCandidateValidatorOzone>
CreateOverlayCandidateValidatorOzone(
    gpu::SurfaceHandle surface_handle,
    const RendererSettings& renderer_settings) {
  if (renderer_settings.overlay_strategies.empty())
    return nullptr;

  auto* overlay_manager = ui::OzonePlatform::GetInstance()->GetOverlayManager();
  std::unique_ptr<ui::OverlayCandidatesOzone> overlay_candidates =
      overlay_manager->CreateOverlayCandidates(surface_handle);
  return std::make_unique<OverlayCandidateValidatorOzone>(
      std::move(overlay_candidates),
      std::move(renderer_settings.overlay_strategies));
}
#endif

#if defined(OS_WIN)
std::unique_ptr<OverlayCandidateValidatorWin>
CreateOverlayCandidateValidatorWin(const ContextProvider* context_provider) {
  DCHECK(context_provider);

  const auto& capabilities = context_provider->ContextCapabilities();
  const bool use_overlays_for_sw_protected_video = base::FeatureList::IsEnabled(
      features::kUseDCOverlaysForSoftwareProtectedVideo);
  const bool use_overlays =
      capabilities.dc_layers && (capabilities.use_dc_overlays_for_video ||
                                 use_overlays_for_sw_protected_video);

  if (use_overlays) {
    return std::make_unique<OverlayCandidateValidatorWin>();
  } else {
    return nullptr;
  }
}
#endif

#if defined(OS_ANDROID)
std::unique_ptr<OverlayCandidateValidatorAndroid>
CreateOverlayCandidateValidatorAndroid(
    const ContextProvider* context_provider) {
  DCHECK(context_provider);
  // When SurfaceControl is enabled, any resource backed by an
  // AHardwareBuffer can be marked as an overlay candidate but it requires
  // that we use a SurfaceControl backed GLSurface. If we're creating a
  // native window backed GLSurface, the overlay processing code will
  // incorrectly assume these resources can be overlaid. So we disable all
  // overlay processing for this OutputSurface.
  const auto& gpu_feature_info = context_provider->GetGpuFeatureInfo();
  const bool allow_overlays =
      gpu_feature_info
          .status_values[gpu::GPU_FEATURE_TYPE_ANDROID_SURFACE_CONTROL] !=
      gpu::kGpuFeatureStatusEnabled;

  if (allow_overlays) {
    return std::make_unique<OverlayCandidateValidatorAndroid>();
  } else {
    return nullptr;
  }
}
#endif
}  // namespace

std::unique_ptr<OverlayCandidateValidator> OverlayCandidateValidator::Create(
    gpu::SurfaceHandle surface_handle,
    const ContextProvider* context_provider,
    const RendererSettings& renderer_settings) {
  if (surface_handle == gpu::kNullSurfaceHandle)
    return nullptr;

  // TODO(weiliangc): Pass in GpuFeatureInfo and ContextCapabilities directly so
  // this class can be used with SkiaRenderer where there is no context
  // provider.
  if (!context_provider)
    return nullptr;

  if (context_provider->ContextCapabilities().surfaceless) {
#if defined(USE_OZONE)
    return CreateOverlayCandidateValidatorOzone(surface_handle,
                                                renderer_settings);
#elif defined(OS_MACOSX)
    return std::make_unique<OverlayCandidateValidatorMac>(
        !renderer_settings.allow_overlays);
#elif defined(OS_ANDROID)
    return std::make_unique<OverlayCandidateValidatorSurfaceControl>();
#else
    NOTREACHED();
#endif
  } else {
#if defined(OS_WIN)
    return CreateOverlayCandidateValidatorWin(context_provider);
#elif defined(OS_ANDROID)
    return CreateOverlayCandidateValidatorAndroid(context_provider);
#else
    return nullptr;
#endif
  }
  return nullptr;
}

OverlayCandidateValidator::OverlayCandidateValidator() = default;
OverlayCandidateValidator::~OverlayCandidateValidator() = default;

bool OverlayCandidateValidator::AttemptWithStrategies(
    const SkMatrix44& output_color_matrix,
    const OverlayProcessor::FilterOperationsMap& render_pass_backdrop_filters,
    DisplayResourceProvider* resource_provider,
    RenderPassList* render_pass_list,
    OverlayCandidateList* candidates,
    std::vector<gfx::Rect>* content_bounds) const {
  for (const auto& strategy : strategies_) {
    if (strategy->Attempt(output_color_matrix, render_pass_backdrop_filters,
                          resource_provider, render_pass_list, candidates,
                          content_bounds)) {
      UMA_HISTOGRAM_ENUMERATION("Viz.DisplayCompositor.OverlayStrategy",
                                strategy->GetUMAEnum());
      return true;
    }
  }
  UMA_HISTOGRAM_ENUMERATION("Viz.DisplayCompositor.OverlayStrategy",
                            OverlayStrategy::kNoStrategyUsed);
  return false;
}

gfx::Rect OverlayCandidateValidator::GetOverlayDamageRectForOutputSurface(
    const OverlayCandidate& candidate) const {
  return ToEnclosedRect(candidate.display_rect);
}

}  // namespace viz
