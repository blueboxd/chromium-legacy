// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_using_strategy.h"

#include <vector>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/overlay_candidate_list.h"
#include "components/viz/service/display/overlay_candidate_validator_strategy.h"
#include "components/viz/service/display/overlay_strategy_single_on_top.h"
#include "components/viz/service/display/overlay_strategy_underlay.h"
#include "components/viz/service/display/skia_output_surface.h"
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
                                    OverlayCandidateList* candidates,
                                    SkiaOutputSurface* skia_output_surface)
      : resource_provider_(resource_provider),
        candidates_(candidates),
        skia_output_surface_(skia_output_surface) {}
  ~SendPromotionHintsBeforeReturning() {
    if (skia_output_surface_) {
      base::flat_set<gpu::Mailbox> promotion_denied;
      base::flat_map<gpu::Mailbox, gfx::Rect> possible_promotions;
      auto locks = candidates_->ConvertLocalPromotionToMailboxKeyed(
          resource_provider_, &promotion_denied, &possible_promotions);

      std::vector<gpu::SyncToken> locks_sync_tokens;
      for (auto& read_lock : locks)
        locks_sync_tokens.push_back(read_lock->sync_token());

      skia_output_surface_->SendOverlayPromotionNotification(
          std::move(locks_sync_tokens), std::move(promotion_denied),
          std::move(possible_promotions));
    } else {
      resource_provider_->SendPromotionHints(
          candidates_->promotion_hint_info_map_,
          candidates_->promotion_hint_requestor_set_);
    }
  }

 private:
  DisplayResourceProvider* resource_provider_;
  OverlayCandidateList* candidates_;
  SkiaOutputSurface* skia_output_surface_;

  DISALLOW_COPY_AND_ASSIGN(SendPromotionHintsBeforeReturning);
};
#endif
}  // namespace

// Default implementation of whether a strategy would remove the output surface
// as overlay plane.
bool OverlayProcessorUsingStrategy::Strategy::RemoveOutputSurfaceAsOverlay() {
  return false;
}

OverlayStrategy OverlayProcessorUsingStrategy::Strategy::GetUMAEnum() const {
  return OverlayStrategy::kUnknown;
}

#if defined(OS_ANDROID)
OverlayProcessorUsingStrategy::OverlayProcessorUsingStrategy(
    SkiaOutputSurface* skia_output_surface,
    std::unique_ptr<OverlayCandidateValidatorStrategy> overlay_validator)
    : OverlayProcessorInterface(),
      overlay_validator_(std::move(overlay_validator)),
      skia_output_surface_(skia_output_surface) {
  if (overlay_validator_)
    overlay_validator_->InitializeStrategies();
}
#else  // defined(USE_OZONE)
OverlayProcessorUsingStrategy::OverlayProcessorUsingStrategy(
    SkiaOutputSurface* skia_output_surface,
    std::unique_ptr<OverlayCandidateValidatorStrategy> overlay_validator)
    : OverlayProcessorInterface(),
      overlay_validator_(std::move(overlay_validator)) {
  if (overlay_validator_)
    overlay_validator_->InitializeStrategies();
}
#endif

OverlayProcessorUsingStrategy::~OverlayProcessorUsingStrategy() = default;

bool OverlayProcessorUsingStrategy::IsOverlaySupported() const {
  return !!overlay_validator_;
}

gfx::Rect OverlayProcessorUsingStrategy::GetAndResetOverlayDamage() {
  gfx::Rect result = overlay_damage_rect_;
  overlay_damage_rect_ = gfx::Rect();
  return result;
}

void OverlayProcessorUsingStrategy::ProcessForOverlays(
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
  TRACE_EVENT0("viz", "OverlayProcessorUsingStrategy::ProcessForOverlays");
#if defined(OS_ANDROID)
  // Be sure to send out notifications, regardless of whether we get to
  // processing for overlays or not.  If we don't, then we should notify that
  // they are not promotable.
  SendPromotionHintsBeforeReturning notifier(resource_provider, candidates,
                                             skia_output_surface_);
#endif

  DCHECK(candidates->empty());

  RenderPass* render_pass = render_passes->back().get();

  // If we have any copy requests, we can't remove any quads for overlays or
  // CALayers because the framebuffer would be missing the removed quads'
  // contents.
  if (!render_pass->copy_requests.empty()) {
    // Reset |previous_frame_underlay_rect_| in case UpdateDamageRect() not
    // being invoked.  Also reset |previous_frame_underlay_was_unoccluded_|.
    previous_frame_underlay_rect_ = gfx::Rect();
    previous_frame_underlay_was_unoccluded_ = false;
    return;
  }

  // Only if that fails, attempt hardware overlay strategies.
  bool success = false;
  if (overlay_validator_) {
    success = overlay_validator_->AttemptWithStrategies(
        output_color_matrix, render_pass_backdrop_filters, resource_provider,
        render_passes, output_surface_plane, candidates, content_bounds);
  }

  if (success) {
    UpdateDamageRect(candidates, previous_frame_underlay_rect_,
                     previous_frame_underlay_was_unoccluded_,
                     &render_pass->quad_list, damage_rect);
  } else {
    if (!previous_frame_underlay_rect_.IsEmpty())
      damage_rect->Union(previous_frame_underlay_rect_);

    DCHECK(candidates->empty());

    previous_frame_underlay_rect_ = gfx::Rect();
    previous_frame_underlay_was_unoccluded_ = false;
  }

  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("viz.debug.overlay_planes"),
                 "Scheduled overlay planes", candidates->size());
}

// Subtract on-top opaque overlays from the damage rect, unless the overlays
// use the backbuffer as their content (in which case, add their combined rect
// back to the damage at the end).
// Also subtract unoccluded underlays from the damage rect if we know that the
// same underlay was scheduled on the previous frame. If the renderer decides
// not to swap the framebuffer there will still be a transparent hole in the
// previous frame.
void OverlayProcessorUsingStrategy::UpdateDamageRect(
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
      // different then that indicates that a different underlay has been
      // chosen and the previous underlay rect should be damaged because it
      // has changed planes from the underlay plane to the main plane. It then
      // checks that this is not a transition from occluded to unoccluded.
      //
      // We also insist that the underlay is unoccluded for at least one frame,
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

void OverlayProcessorUsingStrategy::AdjustOutputSurfaceOverlay(
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
  if (overlay_validator_ &&
      overlay_validator_->StrategyNeedsOutputSurfacePlaneRemoved()) {
    output_surface_plane->reset();
  }
}

bool OverlayProcessorUsingStrategy::NeedsSurfaceOccludingDamageRect() const {
  return overlay_validator_ &&
         overlay_validator_->NeedsSurfaceOccludingDamageRect();
}

void OverlayProcessorUsingStrategy::SetDisplayTransformHint(
    gfx::OverlayTransform transform) {
  if (overlay_validator_)
    overlay_validator_->SetDisplayTransform(transform);
}

void OverlayProcessorUsingStrategy::SetValidatorViewportSize(
    const gfx::Size& size) {
  if (overlay_validator_)
    overlay_validator_->SetViewportSize(size);
}

void OverlayProcessorUsingStrategy::SetSoftwareMirrorMode(
    bool software_mirror_mode) {
  if (overlay_validator_)
    overlay_validator_->SetSoftwareMirrorMode(software_mirror_mode);
}
}  // namespace viz
