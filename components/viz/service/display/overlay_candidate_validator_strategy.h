// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_CANDIDATE_VALIDATOR_STRATEGY_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_CANDIDATE_VALIDATOR_STRATEGY_H_

#include <vector>

#include "components/viz/service/display/overlay_candidate_validator.h"

namespace viz {
class RendererSettings;

// This class can be used to answer questions about possible overlay
// configurations for a particular output device.
class VIZ_SERVICE_EXPORT OverlayCandidateValidatorStrategy
    : public OverlayCandidateValidator {
 public:
  static std::unique_ptr<OverlayCandidateValidatorStrategy> Create(
      gpu::SurfaceHandle surface_handle,
      const OutputSurface::Capabilities& capabilities,
      const RendererSettings& renderer_settings);

  ~OverlayCandidateValidatorStrategy() override;

  // Populates a list of strategies that may work with this validator. Should be
  // called at most once.
  virtual void InitializeStrategies() = 0;

  // Mac doesn't use strategies.
  bool AllowCALayerOverlays() const final;

  // Windows doesn't use strategies.
  bool AllowDCLayerOverlays() const final;

  // A list of possible overlay candidates is presented to this function.
  // The expected result is that those candidates that can be in a separate
  // plane are marked with |overlay_handled| set to true, otherwise they are
  // to be traditionally composited. Candidates with |overlay_handled| set to
  // true must also have their |display_rect| converted to integer
  // coordinates  in physical display coordinates if necessary. When the output
  // surface uses a buffer from |BufferQueue|, it generates a |primary_plane|.
  // The |primary_plane| is always handled, but its information needs to be
  // passed to the hardware overlay system though this function.
  virtual void CheckOverlaySupport(const PrimaryPlane* primary_plane,
                                   OverlayCandidateList* surfaces) = 0;

  // Returns the overlay damage rect covering the main plane rendered by the
  // OutputSurface. This rect is in the same space where the OutputSurface
  // renders the content for the main plane, including the display transform if
  // needed. Should only be called after the overlays are processed.
  virtual gfx::Rect GetOverlayDamageRectForOutputSurface(
      const OverlayCandidate& candidate) const;

  // Iterate through a list of strategies and attempt to overlay with each.
  // Returns true if one of the attempts is successful. Has to be called after
  // InitializeStrategies(). A |primary_plane| represents the output surface's
  // buffer that comes from |BufferQueue|. It is passed in here so it could be
  // pass through to hardware through CheckOverlaySupport. It is not passed in
  // as a const member because the underlay strategy changes the
  // |primary_plane|'s blending setting.
  bool AttemptWithStrategies(
      const SkMatrix44& output_color_matrix,
      const OverlayProcessor::FilterOperationsMap& render_pass_backdrop_filters,
      DisplayResourceProvider* resource_provider,
      RenderPassList* render_pass_list,
      PrimaryPlane* primary_plane,
      OverlayCandidateList* candidates,
      std::vector<gfx::Rect>* content_bounds);

  // If the full screen strategy is successful, we no longer need to overlay the
  // output surface since it will be fully covered.
  bool StrategyNeedsOutputSurfacePlaneRemoved() override;

 protected:
  OverlayCandidateValidatorStrategy();

  OverlayProcessor::StrategyList strategies_;
  OverlayProcessor::Strategy* last_successful_strategy_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_CANDIDATE_VALIDATOR_STRATEGY_H_
