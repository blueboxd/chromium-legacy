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
// TODO(weiliangc): Its functionalities should be merged into subclass of
// OverlayProcessor.
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

 protected:
  OverlayCandidateValidator();
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_CANDIDATE_VALIDATOR_H_
