// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_android.h"

#include "components/viz/common/quads/stream_video_draw_quad.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/overlay_strategy_underlay.h"
#include "components/viz/service/display/skia_output_surface.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace viz {
OverlayProcessorAndroid::OverlayProcessorAndroid(
    SkiaOutputSurface* skia_output_surface,
    bool enable_overlay)
    : OverlayProcessorUsingStrategy(),
      skia_output_surface_(skia_output_surface),
      overlay_enabled_(enable_overlay) {
  if (overlay_enabled_) {
    // For Android, we do not have the ability to skip an overlay, since the
    // texture is already in a SurfaceView.  Ideally, we would honor a 'force
    // overlay' flag that FromDrawQuad would also check.
    // For now, though, just skip the opacity check.  We really have no idea if
    // the underlying overlay is opaque anyway; the candidate is referring to
    // a dummy resource that has no relation to what the overlay contains.
    // https://crbug.com/842931 .
    strategies_.push_back(std::make_unique<OverlayStrategyUnderlay>(
        this, OverlayStrategyUnderlay::OpaqueMode::AllowTransparentCandidates));
  }
}

OverlayProcessorAndroid::~OverlayProcessorAndroid() {}

bool OverlayProcessorAndroid::IsOverlaySupported() const {
  return overlay_enabled_;
}

bool OverlayProcessorAndroid::NeedsSurfaceOccludingDamageRect() const {
  return false;
}

void OverlayProcessorAndroid::CheckOverlaySupport(
    const OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane,
    OverlayCandidateList* candidates) {
  // For pre-SurfaceControl Android we should not have output surface as overlay
  // plane.
  DCHECK(!primary_plane);

  // There should only be at most a single overlay candidate: the
  // video quad.
  // There's no check that the presented candidate is really a video frame for
  // a fullscreen video. Instead it's assumed that if a quad is marked as
  // overlayable, it's a fullscreen video quad.
  DCHECK_LE(candidates->size(), 1u);

  if (!candidates->empty()) {
    OverlayCandidate& candidate = candidates->front();

    // This quad either will be promoted, or would be if it were backed by a
    // SurfaceView.  Record that it should get a promotion hint.
    promotion_hint_info_map_[candidate.resource_id] = candidate.display_rect;

    if (candidate.is_backed_by_surface_texture) {
      // This quad would be promoted if it were backed by a SurfaceView.  Since
      // it isn't, we can't promote it.
      return;
    }

    candidate.display_rect =
        gfx::RectF(gfx::ToEnclosingRect(candidate.display_rect));
    candidate.overlay_handled = true;
    candidate.plane_z_order = -1;

    // This quad will be promoted.  We clear the promotable hints here, since
    // we can only promote a single quad.  Otherwise, somebody might try to
    // back one of the promotable quads with a SurfaceView, and either it or
    // |candidate| would have to fall back to a texture.
    promotion_hint_info_map_.clear();
    promotion_hint_info_map_[candidate.resource_id] = candidate.display_rect;
  }
}
gfx::Rect OverlayProcessorAndroid::GetOverlayDamageRectForOutputSurface(
    const OverlayCandidate& overlay) const {
  return ToEnclosedRect(overlay.display_rect);
}

void OverlayProcessorAndroid::NotifyOverlayPromotion(
    DisplayResourceProvider* resource_provider,
    const CandidateList& candidates,
    const QuadList& quad_list) {
  // No need to notify overlay promotion if not any resource wants promotion
  // hints.
  if (!resource_provider->DoAnyResourcesWantPromotionHints())
    return;

  // |promotion_hint_requestor_set_| is calculated here, so it should be empty
  // to begin with.
  DCHECK(promotion_hint_requestor_set_.empty());

  for (auto* quad : quad_list) {
    if (quad->material != DrawQuad::Material::kStreamVideoContent)
      continue;
    ResourceId id = StreamVideoDrawQuad::MaterialCast(quad)->resource_id();
    if (!resource_provider->DoesResourceWantPromotionHint(id))
      continue;
    promotion_hint_requestor_set_.insert(id);
  }

  if (skia_output_surface_) {
    NotifyOverlayPromotionUsingSkiaOutputSurface(resource_provider, candidates);
  } else {
    resource_provider->SendPromotionHints(promotion_hint_info_map_,
                                          promotion_hint_requestor_set_);
  }
  promotion_hint_info_map_.clear();
  promotion_hint_requestor_set_.clear();
}

void OverlayProcessorAndroid::NotifyOverlayPromotionUsingSkiaOutputSurface(
    DisplayResourceProvider* resource_provider,
    const OverlayCandidateList& candidate_list) {
  base::flat_set<gpu::Mailbox> promotion_denied;
  base::flat_map<gpu::Mailbox, gfx::Rect> possible_promotions;

  DCHECK(candidate_list.empty() || candidate_list.size() == 1u);

  std::vector<
      std::unique_ptr<DisplayResourceProvider::ScopedReadLockSharedImage>>
      locks;
  for (auto& request : promotion_hint_requestor_set_) {
    // If we successfully promote one candidate, then that promotion hint
    // should be sent later when we schedule the overlay.
    if (!candidate_list.empty() &&
        candidate_list.front().resource_id == request)
      continue;

    locks.emplace_back(
        std::make_unique<DisplayResourceProvider::ScopedReadLockSharedImage>(
            resource_provider, request));
    auto iter = promotion_hint_info_map_.find(request);
    if (iter != promotion_hint_info_map_.end()) {
      // This is a possible promotion.
      possible_promotions.emplace(locks.back()->mailbox(),
                                  gfx::ToEnclosedRect(iter->second));
    } else {
      promotion_denied.insert(locks.back()->mailbox());
    }
  }

  std::vector<gpu::SyncToken> locks_sync_tokens;
  for (auto& read_lock : locks)
    locks_sync_tokens.push_back(read_lock->sync_token());

  skia_output_surface_->SendOverlayPromotionNotification(
      std::move(locks_sync_tokens), std::move(promotion_denied),
      std::move(possible_promotions));
}

}  // namespace viz
