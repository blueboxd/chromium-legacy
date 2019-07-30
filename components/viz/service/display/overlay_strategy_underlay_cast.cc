// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_strategy_underlay_cast.h"

#include "base/containers/adapters.h"
#include "base/lazy_instance.h"
#include "base/unguessable_token.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/video_hole_draw_quad.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace viz {
namespace {

base::LazyInstance<OverlayStrategyUnderlayCast::OverlayCompositedCallback>::
    DestructorAtExit g_overlay_composited_callback = LAZY_INSTANCE_INITIALIZER;

}  // namespace

OverlayStrategyUnderlayCast::OverlayStrategyUnderlayCast(
    OverlayCandidateValidator* capability_checker)
    : OverlayStrategyUnderlay(capability_checker) {}

OverlayStrategyUnderlayCast::~OverlayStrategyUnderlayCast() {}

bool OverlayStrategyUnderlayCast::Attempt(
    const SkMatrix44& output_color_matrix,
    const OverlayProcessor::FilterOperationsMap& render_pass_backdrop_filters,
    DisplayResourceProvider* resource_provider,
    RenderPassList* render_pass_list,
    OverlayCandidateList* candidate_list,
    std::vector<gfx::Rect>* content_bounds) {
  RenderPass* render_pass = render_pass_list->back().get();
  QuadList& quad_list = render_pass->quad_list;
  bool found_underlay = false;
  gfx::Rect content_rect;
  for (const auto* quad : base::Reversed(quad_list)) {
    if (OverlayCandidate::IsInvisibleQuad(quad))
      continue;

    const auto& transform = quad->shared_quad_state->quad_to_target_transform;
    gfx::RectF quad_rect = gfx::RectF(quad->rect);
    transform.TransformRect(&quad_rect);

    bool is_underlay = false;
    if (!found_underlay) {
      OverlayCandidate candidate;
      // Look for quads that are overlayable and require an overlay. Chromecast
      // only supports a video underlay so this can't promote all quads that are
      // overlayable, it needs to ensure that the quad requires overlays since
      // that quad is side-channeled through a secure path into an overlay
      // sitting underneath the primary plane. This is only looking at where the
      // quad is supposed to be to replace it with a transparent quad to allow
      // the underlay to be visible.
      // VIDEO_HOLE implies it requires overlay.
      is_underlay =
          quad->material == DrawQuad::Material::kVideoHole &&
          OverlayCandidate::FromDrawQuad(resource_provider, output_color_matrix,
                                         quad, &candidate);
      found_underlay = is_underlay;
    }

    if (!found_underlay && quad->material == DrawQuad::Material::kSolidColor) {
      const SolidColorDrawQuad* solid = SolidColorDrawQuad::MaterialCast(quad);
      if (solid->color == SK_ColorBLACK)
        continue;
    }

    if (is_underlay) {
      content_rect.Subtract(gfx::ToEnclosedRect(quad_rect));
    } else {
      content_rect.Union(gfx::ToEnclosingRect(quad_rect));
    }
  }

  if (is_using_overlay_ != found_underlay) {
    is_using_overlay_ = found_underlay;
    VLOG(1) << (found_underlay ? "Overlay activated" : "Overlay deactivated");
  }

  // If the primary plane shows up in the candidates list make sure it isn't
  // opaque otherwise the content underneath won't be visible.
  if (!candidate_list->empty()) {
    DCHECK_EQ(1u, candidate_list->size());
    DCHECK(candidate_list->front().use_output_surface_for_resource);
    candidate_list->front().is_opaque = false;
  }

  if (found_underlay) {
    for (auto it = quad_list.begin(); it != quad_list.end(); ++it) {
      OverlayCandidate candidate;
      if (it->material != DrawQuad::Material::kVideoHole ||
          !OverlayCandidate::FromDrawQuad(
              resource_provider, output_color_matrix, *it, &candidate)) {
        continue;
      }

      // TODO(guohuideng): activate overlay through MediaServe when it's
      // ready, using |overlay_plane_id|. see b/79266094.
      base::UnguessableToken overlay_plane_id =
          VideoHoleDrawQuad::MaterialCast(*it)->overlay_plane_id;
      ANALYZER_ALLOW_UNUSED(overlay_plane_id);

      render_pass->quad_list.ReplaceExistingQuadWithOpaqueTransparentSolidColor(
          it);

      if (!g_overlay_composited_callback.Get().is_null()) {
        g_overlay_composited_callback.Get().Run(candidate.display_rect,
                                                candidate.transform);
      }
      break;
    }
  }

  DCHECK(content_bounds && content_bounds->empty());
  if (found_underlay) {
    content_bounds->push_back(content_rect);
  }
  return found_underlay;
}

OverlayStrategy OverlayStrategyUnderlayCast::GetUMAEnum() const {
  return OverlayStrategy::kUnderlayCast;
}

// static
void OverlayStrategyUnderlayCast::SetOverlayCompositedCallback(
    const OverlayCompositedCallback& cb) {
  g_overlay_composited_callback.Get() = cb;
}

}  // namespace viz
