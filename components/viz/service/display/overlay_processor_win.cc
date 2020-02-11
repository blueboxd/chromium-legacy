// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_win.h"

#include <vector>

#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/output_surface.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace viz {

OverlayProcessorWin::OverlayProcessorWin(
    bool enable_dc_overlay,
    std::unique_ptr<DCLayerOverlayProcessor> dc_layer_overlay_processor)
    : enable_dc_overlay_(enable_dc_overlay),
      dc_layer_overlay_processor_(std::move(dc_layer_overlay_processor)) {}

OverlayProcessorWin::~OverlayProcessorWin() = default;

bool OverlayProcessorWin::IsOverlaySupported() const {
  return enable_dc_overlay_;
}

gfx::Rect OverlayProcessorWin::GetAndResetOverlayDamage() {
  return gfx::Rect();
}

void OverlayProcessorWin::ProcessForOverlays(
    DisplayResourceProvider* resource_provider,
    RenderPassList* render_passes,
    const SkMatrix44& output_color_matrix,
    const OverlayProcessorInterface::FilterOperationsMap& render_pass_filters,
    const OverlayProcessorInterface::FilterOperationsMap&
        render_pass_backdrop_filters,
    OutputSurfaceOverlayPlane* output_surface_plane,
    CandidateList* candidates,
    gfx::Rect* damage_rect,
    std::vector<gfx::Rect>* content_bounds) {
  TRACE_EVENT0("viz", "OverlayProcessorWin::ProcessForOverlays");

  RenderPass* root_render_pass = render_passes->back().get();
  // Skip overlay processing if we have copy request.
  if (!root_render_pass->copy_requests.empty()) {
    damage_rect->Union(dc_layer_overlay_processor_
                           ->previous_frame_overlay_damage_contribution());
    // Update damage rect before calling ClearOverlayState, otherwise
    // previous_frame_overlay_rect_union will be empty.
    dc_layer_overlay_processor_->ClearOverlayState();
    return;
  }

  // Skip overlay processing if output colorspace is HDR.
  // Since Overlay only supports NV12 and YUY2 now, HDR content (usually P010
  // format) cannot output through overlay without format degrading.
  if (root_render_pass->content_color_usage == gfx::ContentColorUsage::kHDR)
    return;

  if (!enable_dc_overlay_)
    return;

  dc_layer_overlay_processor_->Process(
      resource_provider, gfx::RectF(root_render_pass->output_rect),
      render_passes, damage_rect, candidates);
}

bool OverlayProcessorWin::NeedsSurfaceOccludingDamageRect() const {
  return true;
}

}  // namespace viz
