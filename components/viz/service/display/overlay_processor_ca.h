// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_CA_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_CA_H_

#include "components/viz/service/display/ca_layer_overlay.h"
#include "components/viz/service/display/overlay_processor.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {
class DisplayResourceProvider;

// TODO(weiliangc): Eventually move all the schedule overlay code from
// GLRenderer to inside this class, and then no need to export CALayerOverlay
// class.
class VIZ_SERVICE_EXPORT OverlayProcessorCA : public OverlayProcessor {
 public:
  explicit OverlayProcessorCA(bool allow_ca_layer_overlays);

 protected:
  bool ProcessForOverlayInternal(
      DisplayResourceProvider* resource_provider,
      RenderPassList* render_passes,
      const OverlayProcessor::FilterOperationsMap& render_pass_filters,
      const OverlayProcessor::FilterOperationsMap& render_pass_backdrop_filters,
      OverlayProcessor::CandidateList* overlay_candidates,
      gfx::Rect* damage_rect) override;

  base::Optional<OverlayProcessor::OutputSurfaceOverlayPlane>
  ProcessOutputSurfaceAsOverlay(
      const gfx::Size& viewport_size,
      const gfx::BufferFormat& buffer_format,
      const gfx::ColorSpace& color_space) const override;

 private:
  // Returns true if all quads in the root render pass have been replaced by
  // CALayerOverlays.
  bool ProcessForCALayerOverlays(
      DisplayResourceProvider* resource_provider,
      const gfx::RectF& display_rect,
      const QuadList& quad_list,
      const base::flat_map<RenderPassId, cc::FilterOperations*>&
          render_pass_filters,
      const base::flat_map<RenderPassId, cc::FilterOperations*>&
          render_pass_backdrop_filters,
      CALayerOverlayList* ca_layer_overlays);

  bool allow_ca_layer_overlays_;
  bool output_surface_already_handled_;

  DISALLOW_COPY_AND_ASSIGN(OverlayProcessorCA);
};
}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_CA_H_
