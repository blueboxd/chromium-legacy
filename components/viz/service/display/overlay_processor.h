// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/display/overlay_candidate_validator.h"
#include "components/viz/service/display/overlay_processor_interface.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/ipc/common/surface_handle.h"

#if defined(OS_MACOSX)
#include "components/viz/service/display/ca_layer_overlay.h"
#elif defined(OS_WIN)
#include "components/viz/service/display/dc_layer_overlay.h"
#endif

namespace cc {
class DisplayResourceProvider;
}

namespace viz {
class OverlayCandidateValidator;

// Subclass that implements OverlayProcessorInterface. This is used on Windows
// and Mac platforms where the actual overlay processing is done by an external
// class.
class VIZ_SERVICE_EXPORT OverlayProcessor : public OverlayProcessorInterface {
 public:
#if defined(OS_MACOSX)
  using CandidateList = CALayerOverlayList;
#elif defined(OS_WIN)
  using CandidateList = DCLayerOverlayList;
#endif

  using OverlayValidator = OverlayCandidateValidator;

  explicit OverlayProcessor(
      std::unique_ptr<OverlayValidator> overlay_validator);
  ~OverlayProcessor() override;

  bool IsOverlaySupported() const override;
  gfx::Rect GetAndResetOverlayDamage() override;

  // Returns true if the platform supports hw overlays and surface occluding
  // damage rect needs to be computed since it will be used by overlay
  // processor.
  bool NeedsSurfaceOccludingDamageRect() const override;

  // Attempt to replace quads from the specified root render pass with overlays
  // or CALayers. This must be called every frame.
  void ProcessForOverlays(
      DisplayResourceProvider* resource_provider,
      RenderPassList* render_passes,
      const SkMatrix44& output_color_matrix,
      const FilterOperationsMap& render_pass_filters,
      const FilterOperationsMap& render_pass_backdrop_filters,
      OutputSurfaceOverlayPlane* output_surface_plane,
      CandidateList* overlay_candidates,
      gfx::Rect* damage_rect,
      std::vector<gfx::Rect>* content_bounds) override;

  // For Mac, if we successfully generated a candidate list for CALayerOverlay,
  // we no longer need the |output_surface_plane|. This function takes a pointer
  // to the base::Optional instance so the instance can be reset. This is also
  // used to remove |output_surface_plane| when the successful overlay strategy
  // doesn't need the |output_surface_plane|. SurfaceControl also overrides this
  // function to adjust the rotation.
  // TODO(weiliangc): Internalize the |output_surface_plane| inside the overlay
  // processor.
  void AdjustOutputSurfaceOverlay(
      base::Optional<OutputSurfaceOverlayPlane>* output_surface_plane) override;

#if defined(OS_WIN)
  void InitializeDCOverlayProcessor(
      std::unique_ptr<DCLayerOverlayProcessor> dc_layer_overlay_processor);
#endif

 protected:
  // For testing.
  const OverlayValidator* GetOverlayCandidateValidator() const {
    return overlay_validator_.get();
  }
  std::unique_ptr<OverlayValidator> overlay_validator_;

 private:
  bool ProcessForCALayers(
      DisplayResourceProvider* resource_provider,
      RenderPass* render_pass,
      const FilterOperationsMap& render_pass_filters,
      const FilterOperationsMap& render_pass_backdrop_filters,
      CandidateList* overlay_candidates,
      gfx::Rect* damage_rect);
  bool ProcessForDCLayers(
      DisplayResourceProvider* resource_provider,
      RenderPassList* render_passes,
      const FilterOperationsMap& render_pass_filters,
      const FilterOperationsMap& render_pass_backdrop_filters,
      CandidateList* overlay_candidates,
      gfx::Rect* damage_rect);

#if defined(OS_WIN)
  std::unique_ptr<DCLayerOverlayProcessor> dc_layer_overlay_processor_;
#endif

  bool output_surface_already_handled_;
  DISALLOW_COPY_AND_ASSIGN(OverlayProcessor);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_H_
