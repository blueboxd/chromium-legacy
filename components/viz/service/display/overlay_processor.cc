// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor.h"

#include <vector>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/service/display/dc_layer_overlay.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/overlay_candidate_validator.h"
#include "components/viz/service/display/overlay_strategy_single_on_top.h"
#include "components/viz/service/display/overlay_strategy_underlay.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/transform.h"

namespace viz {

namespace {

#if defined(OS_ANDROID)
// Utility class to make sure that we notify resource that they're promotable
// before returning from ProcessForOverlays.
class SendPromotionHintsBeforeReturning {
 public:
  SendPromotionHintsBeforeReturning(DisplayResourceProvider* resource_provider,
                                    OverlayCandidateList* candidates)
      : resource_provider_(resource_provider), candidates_(candidates) {}
  ~SendPromotionHintsBeforeReturning() {
    resource_provider_->SendPromotionHints(
        candidates_->promotion_hint_info_map_,
        candidates_->promotion_hint_requestor_set_);
  }

 private:
  DisplayResourceProvider* resource_provider_;
  OverlayCandidateList* candidates_;

  DISALLOW_COPY_AND_ASSIGN(SendPromotionHintsBeforeReturning);
};
#endif

}  // namespace

// Default implementation of whether a strategy would remove the output surface
// as overlay plane.
bool OverlayProcessor::Strategy::RemoveOutputSurfaceAsOverlay() {
  return false;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class UnderlayDamage {
  kZeroDamageRect,
  kNonOccludingDamageOnly,
  kOccludingDamageOnly,
  kOccludingAndNonOccludingDamages,
  kMaxValue = kOccludingAndNonOccludingDamages,
};

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

OverlayStrategy OverlayProcessor::Strategy::GetUMAEnum() const {
  return OverlayStrategy::kUnknown;
}

std::unique_ptr<OverlayProcessor> OverlayProcessor::CreateOverlayProcessor(
    const OutputSurface& output_surface,
    const RendererSettings& renderer_settings) {
  return base::WrapUnique(new OverlayProcessor(
      OverlayCandidateValidator::Create(output_surface.GetSurfaceHandle(),
                                        output_surface, renderer_settings),
      std::make_unique<DCLayerOverlayProcessor>(output_surface.capabilities(),
                                                renderer_settings)));
}

OverlayProcessor::OverlayProcessor(
    std::unique_ptr<OverlayCandidateValidator> overlay_validator,
    std::unique_ptr<DCLayerOverlayProcessor> dc_layer_overlay_processor)
    : overlay_validator_(std::move(overlay_validator)),
      dc_layer_overlay_processor_(std::move(dc_layer_overlay_processor)) {
  DCHECK(dc_layer_overlay_processor_);
  if (overlay_validator_)
    overlay_validator_->InitializeStrategies();
}

// For testing.
OverlayProcessor::OverlayProcessor(
    std::unique_ptr<OverlayCandidateValidator> overlay_validator)
    : OverlayProcessor(std::move(overlay_validator),
                       std::make_unique<DCLayerOverlayProcessor>()) {}

OverlayProcessor::~OverlayProcessor() = default;

gfx::Rect OverlayProcessor::GetAndResetOverlayDamage() {
  gfx::Rect result = overlay_damage_rect_;
  overlay_damage_rect_ = gfx::Rect();
  return result;
}

bool OverlayProcessor::ProcessForCALayers(
    DisplayResourceProvider* resource_provider,
    RenderPass* render_pass,
    const OverlayProcessor::FilterOperationsMap& render_pass_filters,
    const OverlayProcessor::FilterOperationsMap& render_pass_backdrop_filters,
    CALayerOverlayList* ca_layer_overlays,
    gfx::Rect* damage_rect) {
  if (!overlay_validator_ || !overlay_validator_->AllowCALayerOverlays())
    return false;

  if (!ProcessForCALayerOverlays(
          resource_provider, gfx::RectF(render_pass->output_rect),
          render_pass->quad_list, render_pass_filters,
          render_pass_backdrop_filters, ca_layer_overlays))
    return false;

  // CALayer overlays are all-or-nothing. If all quads were replaced with
  // layers then clear the list and remove the backbuffer from the overcandidate
  // list.
  output_surface_already_handled_ = true;
  overlay_damage_rect_ = render_pass->output_rect;
  *damage_rect = gfx::Rect();
  return true;
}

bool OverlayProcessor::ProcessForDCLayers(
    DisplayResourceProvider* resource_provider,
    RenderPassList* render_passes,
    const OverlayProcessor::FilterOperationsMap& render_pass_filters,
    const OverlayProcessor::FilterOperationsMap& render_pass_backdrop_filters,
    DCLayerOverlayList* dc_layer_overlays,
    gfx::Rect* damage_rect) {
  if (!overlay_validator_ || !overlay_validator_->AllowDCLayerOverlays())
    return false;

  dc_layer_overlay_processor_->Process(
      resource_provider, gfx::RectF(render_passes->back()->output_rect),
      render_passes, damage_rect, dc_layer_overlays);
  return true;
}

void OverlayProcessor::ProcessForOverlays(
    DisplayResourceProvider* resource_provider,
    RenderPassList* render_passes,
    const SkMatrix44& output_color_matrix,
    const OverlayProcessor::FilterOperationsMap& render_pass_filters,
    const OverlayProcessor::FilterOperationsMap& render_pass_backdrop_filters,
    OutputSurfaceOverlayPlane* output_surface_plane,
    OverlayCandidateList* candidates,
    CALayerOverlayList* ca_layer_overlays,
    DCLayerOverlayList* dc_layer_overlays,
    gfx::Rect* damage_rect,
    std::vector<gfx::Rect>* content_bounds) {
  TRACE_EVENT0("viz", "OverlayProcessor::ProcessForOverlays");
#if defined(OS_ANDROID)
  // Be sure to send out notifications, regardless of whether we get to
  // processing for overlays or not.  If we don't, then we should notify that
  // they are not promotable.
  SendPromotionHintsBeforeReturning notifier(resource_provider, candidates);
#endif

  // Clear to get ready to handle output surface as overlay.
  output_surface_already_handled_ = false;

  // Reset |previous_frame_underlay_rect_| in case UpdateDamageRect() not being
  // invoked.  Also reset |previous_frame_underlay_was_unoccluded_|.
  const gfx::Rect previous_frame_underlay_rect = previous_frame_underlay_rect_;
  previous_frame_underlay_rect_ = gfx::Rect();
  bool previous_frame_underlay_was_unoccluded =
      previous_frame_underlay_was_unoccluded_;
  previous_frame_underlay_was_unoccluded_ = false;

  RenderPass* root_render_pass = render_passes->back().get();

  // If we have any copy requests, we can't remove any quads for overlays,
  // CALayers, or DCLayers because the framebuffer would be missing the removed
  // quads' contents.
  if (!root_render_pass->copy_requests.empty()) {
    damage_rect->Union(dc_layer_overlay_processor_
                           ->previous_frame_overlay_damage_contribution());
    // Update damage rect before calling ClearOverlayState, otherwise
    // previous_frame_overlay_rect_union will be empty.
    dc_layer_overlay_processor_->ClearOverlayState();
    return;
  }

  // First attempt to process for CALayers.
  if (ProcessForCALayers(resource_provider, root_render_pass,
                         render_pass_filters, render_pass_backdrop_filters,
                         ca_layer_overlays, damage_rect)) {
    return;
  }

  if (ProcessForDCLayers(resource_provider, render_passes, render_pass_filters,
                         render_pass_backdrop_filters, dc_layer_overlays,
                         damage_rect)) {
    return;
  }

  DCHECK(candidates->empty());
  // Only if that fails, attempt hardware overlay strategies.
  bool success = false;
  if (overlay_validator_) {
    success = overlay_validator_->AttemptWithStrategies(
        output_color_matrix, render_pass_backdrop_filters, resource_provider,
        render_passes, output_surface_plane, candidates, content_bounds);
  }

  if (success) {
    UpdateDamageRect(candidates, previous_frame_underlay_rect,
                     previous_frame_underlay_was_unoccluded,
                     &root_render_pass->quad_list, damage_rect);
  } else {
    if (!previous_frame_underlay_rect.IsEmpty())
      damage_rect->Union(previous_frame_underlay_rect);

    DCHECK(candidates->empty());
  }

  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("viz.debug.overlay_planes"),
                 "Scheduled overlay planes", candidates->size());
}

// Subtract on-top opaque overlays from the damage rect, unless the overlays use
// the backbuffer as their content (in which case, add their combined rect
// back to the damage at the end).
// Also subtract unoccluded underlays from the damage rect if we know that the
// same underlay was scheduled on the previous frame. If the renderer decides
// not to swap the framebuffer there will still be a transparent hole in the
// previous frame.
void OverlayProcessor::UpdateDamageRect(
    OverlayCandidateList* candidates,
    const gfx::Rect& previous_frame_underlay_rect,
    bool previous_frame_underlay_was_unoccluded,
    const QuadList* quad_list,
    gfx::Rect* damage_rect) {
  gfx::Rect this_frame_underlay_rect;
  for (const OverlayCandidate& overlay : *candidates) {
    if (overlay.plane_z_order >= 0) {
      const gfx::Rect overlay_display_rect =
          overlay_validator_->GetOverlayDamageRectForOutputSurface(overlay);
      // If an overlay candidate comes from output surface, its z-order should
      // be 0.
      overlay_damage_rect_.Union(overlay_display_rect);
      if (overlay.is_opaque)
        damage_rect->Subtract(overlay_display_rect);
    } else {
      // Process underlay candidates:
      // Track the underlay_rect from frame to frame. If it is the same and
      // nothing is on top of it then that rect doesn't need to be damaged
      // because the drawing is occurring on a different plane. If it is
      // different then that indicates that a different underlay has been chosen
      // and the previous underlay rect should be damaged because it has changed
      // planes from the underlay plane to the main plane.
      // It then checks that this is not a transition from occluded to
      // unoccluded.
      //
      // We also insist that the underlay is unoccluded for at leat one frame,
      // else when content above the overlay transitions from not fully
      // transparent to fully transparent, we still need to erase it from the
      // framebuffer.  Otherwise, the last non-transparent frame will remain.
      // https://crbug.com/875879
      // However, if the underlay is unoccluded, we check if the damage is due
      // to a solid-opaque-transparent quad. If so, then we subtract this
      // damage.
      this_frame_underlay_rect =
          overlay_validator_->GetOverlayDamageRectForOutputSurface(overlay);

      bool same_underlay_rect =
          this_frame_underlay_rect == previous_frame_underlay_rect;
      bool transition_from_occluded_to_unoccluded =
          overlay.is_unoccluded && !previous_frame_underlay_was_unoccluded;
      bool always_unoccluded =
          overlay.is_unoccluded && previous_frame_underlay_was_unoccluded;

      if (same_underlay_rect && !transition_from_occluded_to_unoccluded &&
          (always_unoccluded || overlay.no_occluding_damage)) {
        damage_rect->Subtract(this_frame_underlay_rect);
      }
      previous_frame_underlay_was_unoccluded_ = overlay.is_unoccluded;
    }

    if (overlay.plane_z_order) {
      RecordOverlayDamageRectHistograms(
          (overlay.plane_z_order > 0), !overlay.no_occluding_damage,
          damage_rect->IsEmpty(),
          false /* occluding_damage_equal_to_damage_rect */);
    }
  }

  if (this_frame_underlay_rect != previous_frame_underlay_rect)
    damage_rect->Union(previous_frame_underlay_rect);

  previous_frame_underlay_rect_ = this_frame_underlay_rect;
}

OverlayProcessor::OutputSurfaceOverlayPlane
OverlayProcessor::ProcessOutputSurfaceAsOverlay(
    const gfx::Size& viewport_size,
    const gfx::BufferFormat& buffer_format,
    const gfx::ColorSpace& color_space,
    bool has_alpha) const {
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

  // This is used by the surface control implementation to adjust the display
  // transform and the display rect.
  if (overlay_validator_)
    overlay_validator_->AdjustOutputSurfaceOverlay(
        &(output_surface_plane->value()));

  // If the overlay candidates cover the entire screen, the
  // |output_surface_plane| could be removed.
  if (overlay_validator_) {
    output_surface_already_handled_ |=
        overlay_validator_->StrategyNeedsOutputSurfacePlaneRemoved();
  }

  if (output_surface_already_handled_)
    output_surface_plane->reset();
}

bool OverlayProcessor::NeedsSurfaceOccludingDamageRect() const {
  return overlay_validator_ &&
         overlay_validator_->NeedsSurfaceOccludingDamageRect();
}

void OverlayProcessor::SetDisplayTransformHint(
    gfx::OverlayTransform transform) {
  if (overlay_validator_)
    overlay_validator_->SetDisplayTransform(transform);
}

void OverlayProcessor::SetValidatorViewportSize(const gfx::Size& size) {
  if (overlay_validator_)
    overlay_validator_->SetViewportSize(size);
}

void OverlayProcessor::SetSoftwareMirrorMode(bool software_mirror_mode) {
  if (overlay_validator_)
    overlay_validator_->SetSoftwareMirrorMode(software_mirror_mode);
}
}  // namespace viz
