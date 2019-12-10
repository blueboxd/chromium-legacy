// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor.h"

#include <vector>

#include "base/metrics/histogram_macros.h"
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
namespace {
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class UnderlayDamage {
  kZeroDamageRect,
  kNonOccludingDamageOnly,
  kOccludingDamageOnly,
  kOccludingAndNonOccludingDamages,
  kMaxValue = kOccludingAndNonOccludingDamages,
};
}  // namespace

// Record UMA histograms for overlays
// 1. Underlay vs. Overlay
// 2. Full screen mode vs. Non Full screen (Windows) mode
// 3. Overlay zero damage rect vs. non zero damage rect
// 4. Underlay zero damage rect, non-zero damage rect with non-occluding damage
//   only, non-zero damage rect with occluding damage, and non-zero damage rect
//   with both damages

// static
void OverlayProcessor::RecordOverlayDamageRectHistograms(
    bool is_overlay,
    bool has_occluding_surface_damage,
    bool zero_damage_rect,
    bool occluding_damage_equal_to_damage_rect) {
  if (is_overlay) {
    UMA_HISTOGRAM_BOOLEAN("Viz.DisplayCompositor.RootDamageRect.Overlay",
                          !zero_damage_rect);
  } else {  // underlay
    UnderlayDamage underlay_damage = UnderlayDamage::kZeroDamageRect;
    if (zero_damage_rect) {
      underlay_damage = UnderlayDamage::kZeroDamageRect;
    } else {
      if (has_occluding_surface_damage) {
        if (occluding_damage_equal_to_damage_rect) {
          underlay_damage = UnderlayDamage::kOccludingDamageOnly;
        } else {
          underlay_damage = UnderlayDamage::kOccludingAndNonOccludingDamages;
        }
      } else {
        underlay_damage = UnderlayDamage::kNonOccludingDamageOnly;
      }
    }
    UMA_HISTOGRAM_ENUMERATION("Viz.DisplayCompositor.RootDamageRect.Underlay",
                              underlay_damage);
  }
}


std::unique_ptr<OverlayProcessor> OverlayProcessor::CreateOverlayProcessor(
    SkiaOutputSurface* skia_output_surface,
    gpu::SurfaceHandle surface_handle,
    const OutputSurface::Capabilities& capabilities,
    const RendererSettings& renderer_settings) {
#if defined(OS_MACOSX) || defined(OS_WIN)
  auto validator = OverlayCandidateValidator::Create(
      surface_handle, capabilities, renderer_settings);
  auto processor = base::WrapUnique(new OverlayProcessor(std::move(validator)));
#else  // defined(USE_OZONE) || defined(OS_ANDROID) || Default
  auto validator = OverlayCandidateValidatorStrategy::Create(
      surface_handle, capabilities, renderer_settings);
  auto processor = base::WrapUnique(new OverlayProcessorUsingStrategy(
      skia_output_surface, std::move(validator)));
#endif

#if defined(OS_WIN)
  processor->InitializeDCOverlayProcessor(
      std::make_unique<DCLayerOverlayProcessor>(capabilities,
                                                renderer_settings));
#endif
  return processor;
}

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

OverlayProcessor::OutputSurfaceOverlayPlane
OverlayProcessor::ProcessOutputSurfaceAsOverlay(
    const gfx::Size& viewport_size,
    const gfx::BufferFormat& buffer_format,
    const gfx::ColorSpace& color_space,
    bool has_alpha) {
  OutputSurfaceOverlayPlane overlay_plane;
  overlay_plane.transform = gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE;
  overlay_plane.resource_size = viewport_size;
  overlay_plane.format = buffer_format;
  overlay_plane.color_space = color_space;
  overlay_plane.enable_blending = has_alpha;

  // Adjust transformation and display_rect based on display rotation.
  overlay_plane.display_rect =
      gfx::RectF(viewport_size.width(), viewport_size.height());

#if defined(ALWAYS_ENABLE_BLENDING_FOR_PRIMARY)
  // On Chromecast, always use RGBA as the scanout format for the primary plane.
  overlay_plane.enable_blending = true;
#endif
  return overlay_plane;
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
