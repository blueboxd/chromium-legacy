// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_CANDIDATE_VALIDATOR_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_CANDIDATE_VALIDATOR_H_

#include <vector>

#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display/overlay_processor.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {
class OutputSurface;
class RendererSettings;

// This class that can be used to answer questions about possible overlay
// configurations for a particular output device. For Mac and Windows validator
// implementations, this class has enough API to answer questions. Android and
// Ozone validators requires a list of overlay strategies for their
// implementations.
class VIZ_SERVICE_EXPORT OverlayCandidateValidator {
 public:
  static std::unique_ptr<OverlayCandidateValidator> Create(
      gpu::SurfaceHandle surface_handle,
      const OutputSurface::Capabilities& capabilities,
      const RendererSettings& renderer_settings);

  virtual ~OverlayCandidateValidator();

  // Returns true if draw quads can be represented as CALayers (Mac only).
  virtual bool AllowCALayerOverlays() const = 0;

  // Returns true if draw quads can be represented as Direct Composition
  // Visuals (Windows only).
  virtual bool AllowDCLayerOverlays() const = 0;

  // Returns true if the platform supports hw overlays and surface occluding
  // damage rect needs to be computed since it will be used by overlay
  // processor.
  virtual bool NeedsSurfaceOccludingDamageRect() const = 0;

  // The following functions are only override by
  // OverlayCandidateValidatorStrategy, but is here to provide the correct
  // interface.
  // A primary plane is generated when the output surface's buffer is supplied
  // by |BufferQueue|. This is considered as an overlay plane.
  using PrimaryPlane = OverlayProcessor::OutputSurfaceOverlayPlane;

  // Set the overlay display transform and viewport size. Value only used for
  // Android Surface Control.
  virtual void SetDisplayTransform(gfx::OverlayTransform transform) {}
  virtual void SetViewportSize(const gfx::Size& size) {}

  // Disables overlays when software mirroring display. This only needs to be
  // implemented for Chrome OS.
  virtual void SetSoftwareMirrorMode(bool enabled) {}

  // This is used to adjust properties of the |primary_plane|, which is the
  // overlay candidate for the output surface. This is called after we process
  // for overlay. Surface Control uses this function to adjust the display
  // transform and display rect.
  virtual void AdjustOutputSurfaceOverlay(PrimaryPlane* output_surface_plane) {}

  // If the full screen strategy is successful, we no longer need to overlay the
  // output surface since it will be fully covered.
  virtual bool StrategyNeedsOutputSurfacePlaneRemoved();

 protected:
  OverlayCandidateValidator();
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_CANDIDATE_VALIDATOR_H_
