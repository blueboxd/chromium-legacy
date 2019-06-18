// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/overlay_candidate_validator_ozone.h"

#include <stddef.h>

#include <utility>

#include "components/viz/service/display/overlay_strategy_fullscreen.h"
#include "components/viz/service/display/overlay_strategy_single_on_top.h"
#include "components/viz/service/display/overlay_strategy_underlay.h"
#include "components/viz/service/display/overlay_strategy_underlay_cast.h"

namespace viz {

// |overlay_candidates| is an object used to answer questions about possible
// overlays configurations.
// |available_strategies| is a list of overlay strategies that should be
// initialized by InitializeStrategies.
OverlayCandidateValidatorOzone::OverlayCandidateValidatorOzone(
    std::unique_ptr<ui::OverlayCandidatesOzone> overlay_candidates,
    std::vector<OverlayStrategy> available_strategies)
    : overlay_candidates_(std::move(overlay_candidates)),
      available_strategies_(std::move(available_strategies)) {}

OverlayCandidateValidatorOzone::~OverlayCandidateValidatorOzone() {}

void OverlayCandidateValidatorOzone::InitializeStrategies() {
  for (OverlayStrategy strategy : available_strategies_) {
    switch (strategy) {
      case OverlayStrategy::kFullscreen:
        strategies_.push_back(
            std::make_unique<OverlayStrategyFullscreen>(this));
        break;
      case OverlayStrategy::kSingleOnTop:
        strategies_.push_back(
            std::make_unique<OverlayStrategySingleOnTop>(this));
        break;
      case OverlayStrategy::kUnderlay:
        strategies_.push_back(std::make_unique<OverlayStrategyUnderlay>(this));
        break;
      case OverlayStrategy::kUnderlayCast:
        strategies_.push_back(
            std::make_unique<OverlayStrategyUnderlayCast>(this));
        break;
      default:
        NOTREACHED();
    }
  }
}

bool OverlayCandidateValidatorOzone::AllowCALayerOverlays() const {
  return false;
}

bool OverlayCandidateValidatorOzone::AllowDCLayerOverlays() const {
  return false;
}

bool OverlayCandidateValidatorOzone::NeedsSurfaceOccludingDamageRect() const {
  return true;
}

void OverlayCandidateValidatorOzone::CheckOverlaySupport(
    OverlayCandidateList* surfaces) {
  // SW mirroring copies out of the framebuffer, so we can't remove any
  // quads for overlaying, otherwise the output is incorrect.
  if (software_mirror_active_) {
    for (size_t i = 0; i < surfaces->size(); i++) {
      surfaces->at(i).overlay_handled = false;
    }
    return;
  }

  DCHECK_GE(2U, surfaces->size());
  ui::OverlayCandidatesOzone::OverlaySurfaceCandidateList ozone_surface_list;
  ozone_surface_list.resize(surfaces->size());

  for (size_t i = 0; i < surfaces->size(); i++) {
    ozone_surface_list.at(i).transform = surfaces->at(i).transform;
    ozone_surface_list.at(i).format = surfaces->at(i).format;
    ozone_surface_list.at(i).display_rect = surfaces->at(i).display_rect;
    ozone_surface_list.at(i).crop_rect = surfaces->at(i).uv_rect;
    ozone_surface_list.at(i).clip_rect = surfaces->at(i).clip_rect;
    ozone_surface_list.at(i).is_clipped = surfaces->at(i).is_clipped;
    ozone_surface_list.at(i).plane_z_order = surfaces->at(i).plane_z_order;
    ozone_surface_list.at(i).buffer_size =
        surfaces->at(i).resource_size_in_pixels;
  }

  overlay_candidates_->CheckOverlaySupport(&ozone_surface_list);
  DCHECK_EQ(surfaces->size(), ozone_surface_list.size());

  for (size_t i = 0; i < surfaces->size(); i++) {
    surfaces->at(i).overlay_handled = ozone_surface_list.at(i).overlay_handled;
    surfaces->at(i).display_rect = ozone_surface_list.at(i).display_rect;
  }
}

void OverlayCandidateValidatorOzone::SetSoftwareMirrorMode(bool enabled) {
  software_mirror_active_ = enabled;
}

}  // namespace viz
