// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor.h"

#include <vector>

#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/overlay_candidate_list.h"
#include "components/viz/service/display/skia_output_surface.h"
#include "ui/gfx/geometry/rect_conversions.h"

#if defined(OS_WIN)
#include "components/viz/service/display/dc_layer_overlay.h"
#endif

#if !defined(OS_WIN) && !defined(OS_MACOSX)
#include "components/viz/service/display/overlay_candidate_validator_strategy.h"
#include "components/viz/service/display/overlay_processor_using_strategy.h"
#endif

namespace viz {
// For testing.
OverlayProcessor::OverlayProcessor(
    std::unique_ptr<OverlayValidator> overlay_validator)
    : overlay_validator_(std::move(overlay_validator)) {
#if defined(OS_WIN)
  InitializeDCOverlayProcessor(std::make_unique<DCLayerOverlayProcessor>());
#endif
}

#if defined(OS_WIN)
void OverlayProcessor::InitializeDCOverlayProcessor(
    std::unique_ptr<DCLayerOverlayProcessor> dc_layer_overlay_processor) {
  dc_layer_overlay_processor_.swap(dc_layer_overlay_processor);
}
#endif  // defined(OS_WIN)

OverlayProcessor::~OverlayProcessor() = default;

bool OverlayProcessor::IsOverlaySupported() const {
  return !!overlay_validator_;
}

gfx::Rect OverlayProcessor::GetAndResetOverlayDamage() {
  return gfx::Rect();
}

bool OverlayProcessor::ProcessForCALayers(
    DisplayResourceProvider* resource_provider,
    RenderPass* render_pass,
    const OverlayProcessor::FilterOperationsMap& render_pass_filters,
    const OverlayProcessor::FilterOperationsMap& render_pass_backdrop_filters,
    CandidateList* ca_layer_overlays,
    gfx::Rect* damage_rect) {
#if defined(OS_MACOSX)
  // Skip overlay processing if we have copy request.
  if (!render_pass->copy_requests.empty())
    return true;

  if (!overlay_validator_ || !overlay_validator_->AllowCALayerOverlays())
    return false;

  if (!ProcessForCALayerOverlays(
          resource_provider, gfx::RectF(render_pass->output_rect),
          render_pass->quad_list, render_pass_filters,
          render_pass_backdrop_filters, ca_layer_overlays))
    return false;

  // CALayer overlays are all-or-nothing. If all quads were replaced with
  // layers then mark the output surface as already handled.
  output_surface_already_handled_ = true;
  *damage_rect = gfx::Rect();
  return true;
#endif  // defined(OS_MACOSX)
  return false;
}

bool OverlayProcessor::ProcessForDCLayers(
    DisplayResourceProvider* resource_provider,
    RenderPassList* render_passes,
    const OverlayProcessor::FilterOperationsMap& render_pass_filters,
    const OverlayProcessor::FilterOperationsMap& render_pass_backdrop_filters,
    CandidateList* dc_layer_overlays,
    gfx::Rect* damage_rect) {
#if defined(OS_WIN)
  // Skip overlay processing if we have copy request.
  if (!render_passes->back()->copy_requests.empty()) {
    damage_rect->Union(dc_layer_overlay_processor_
                           ->previous_frame_overlay_damage_contribution());
    // Update damage rect before calling ClearOverlayState, otherwise
    // previous_frame_overlay_rect_union will be empty.
    dc_layer_overlay_processor_->ClearOverlayState();
    return true;
  }

  if (!overlay_validator_ || !overlay_validator_->AllowDCLayerOverlays())
    return false;

  dc_layer_overlay_processor_->Process(
      resource_provider, gfx::RectF(render_passes->back()->output_rect),
      render_passes, damage_rect, dc_layer_overlays);
  return true;
#endif  // defined(OS_WIN)
  return false;
}

void OverlayProcessor::ProcessForOverlays(
    DisplayResourceProvider* resource_provider,
    RenderPassList* render_passes,
    const SkMatrix44& output_color_matrix,
    const OverlayProcessor::FilterOperationsMap& render_pass_filters,
    const OverlayProcessor::FilterOperationsMap& render_pass_backdrop_filters,
    OutputSurfaceOverlayPlane* output_surface_plane,
    CandidateList* candidates,
    gfx::Rect* damage_rect,
    std::vector<gfx::Rect>* content_bounds) {
  TRACE_EVENT0("viz", "OverlayProcessor::ProcessForOverlays");
  // Clear to get ready to handle output surface as overlay.
  output_surface_already_handled_ = false;

  // First attempt to process for CALayers.
  if (ProcessForCALayers(resource_provider, render_passes->back().get(),
                         render_pass_filters, render_pass_backdrop_filters,
                         candidates, damage_rect)) {
    return;
  }

  if (ProcessForDCLayers(resource_provider, render_passes, render_pass_filters,
                         render_pass_backdrop_filters, candidates,
                         damage_rect)) {
    return;
  }
}

void OverlayProcessor::AdjustOutputSurfaceOverlay(
    base::Optional<OutputSurfaceOverlayPlane>* output_surface_plane) {
  if (!output_surface_plane->has_value())
    return;

  if (output_surface_already_handled_)
    output_surface_plane->reset();
}

bool OverlayProcessor::NeedsSurfaceOccludingDamageRect() const {
  return overlay_validator_ &&
         overlay_validator_->NeedsSurfaceOccludingDamageRect();
}

}  // namespace viz
