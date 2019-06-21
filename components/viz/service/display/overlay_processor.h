// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "components/viz/common/display/overlay_strategy.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/service/display/ca_layer_overlay.h"
#include "components/viz/service/display/dc_layer_overlay.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/ipc/common/surface_handle.h"

namespace cc {
class DisplayResourceProvider;
}

namespace viz {
class OverlayCandidateValidator;
class RendererSettings;
class ContextProvider;

class VIZ_SERVICE_EXPORT OverlayProcessor {
 public:
  using FilterOperationsMap =
      base::flat_map<RenderPassId, cc::FilterOperations*>;

  static void RecordOverlayDamageRectHistograms(
      bool is_overlay,
      bool has_occluding_surface_damage,
      bool zero_damage_rect,
      bool occluding_damage_equal_to_damage_rect);

  class VIZ_SERVICE_EXPORT Strategy {
   public:
    virtual ~Strategy() {}
    // Returns false if the strategy cannot be made to work with the
    // current set of render passes. Returns true if the strategy was successful
    // and adds any additional passes necessary to represent overlays to
    // |render_pass_list|. Most strategies should look at the primary
    // RenderPass, the last element.
    virtual bool Attempt(
        const SkMatrix44& output_color_matrix,
        const FilterOperationsMap& render_pass_backdrop_filters,
        DisplayResourceProvider* resource_provider,
        RenderPassList* render_pass_list,
        OverlayCandidateList* candidates,
        std::vector<gfx::Rect>* content_bounds) = 0;

    virtual OverlayStrategy GetUMAEnum() const;
  };
  using StrategyList = std::vector<std::unique_ptr<Strategy>>;

  static std::unique_ptr<OverlayProcessor> CreateOverlayProcessor(
      const ContextProvider* context_provider,
      gpu::SurfaceHandle surface_handle,
      const RendererSettings& renderer_settings);

  virtual ~OverlayProcessor();

  gfx::Rect GetAndResetOverlayDamage();
  void SetSoftwareMirrorMode(bool software_mirror_mode);

  const OverlayCandidateValidator* GetOverlayCandidateValidator() const {
    return overlay_validator_.get();
  }

  bool NeedsSurfaceOccludingDamageRect() const;
  void SetDisplayTransformHint(gfx::OverlayTransform transform);
  void SetValidatorViewportSize(const gfx::Size& size);

  // Attempt to replace quads from the specified root render pass with overlays
  // or CALayers. This must be called every frame.
  void ProcessForOverlays(
      DisplayResourceProvider* resource_provider,
      RenderPassList* render_passes,
      const SkMatrix44& output_color_matrix,
      const FilterOperationsMap& render_pass_filters,
      const FilterOperationsMap& render_pass_backdrop_filters,
      OverlayCandidateList* overlay_candidates,
      CALayerOverlayList* ca_layer_overlays,
      DCLayerOverlayList* dc_layer_overlays,
      gfx::Rect* damage_rect,
      std::vector<gfx::Rect>* content_bounds);

  void SetDCHasHwOverlaySupportForTesting() {
    dc_processor_->SetHasHwOverlaySupport();
  }

 protected:
  explicit OverlayProcessor(const ContextProvider* context_provider);
  void SetOverlayCandidateValidator(
      std::unique_ptr<OverlayCandidateValidator> overlay_validator);

  StrategyList strategies_;
  std::unique_ptr<OverlayCandidateValidator> overlay_validator_;
  gfx::Rect overlay_damage_rect_;
  gfx::Rect previous_frame_underlay_rect_;
  bool previous_frame_underlay_was_unoccluded_ = false;

 private:
  bool ProcessForCALayers(
      DisplayResourceProvider* resource_provider,
      RenderPass* render_pass,
      const FilterOperationsMap& render_pass_filters,
      const FilterOperationsMap& render_pass_backdrop_filters,
      OverlayCandidateList* overlay_candidates,
      CALayerOverlayList* ca_layer_overlays,
      gfx::Rect* damage_rect);
  bool ProcessForDCLayers(
      DisplayResourceProvider* resource_provider,
      RenderPassList* render_passes,
      const FilterOperationsMap& render_pass_filters,
      const FilterOperationsMap& render_pass_backdrop_filters,
      OverlayCandidateList* overlay_candidates,
      DCLayerOverlayList* dc_layer_overlays,
      gfx::Rect* damage_rect);
  // Update |damage_rect| by removing damage casued by |candidates|.
  void UpdateDamageRect(OverlayCandidateList* candidates,
                        const gfx::Rect& previous_frame_underlay_rect,
                        bool previous_frame_underlay_was_unoccluded,
                        const QuadList* quad_list,
                        gfx::Rect* damage_rect);

  std::unique_ptr<DCLayerOverlayProcessor> dc_processor_;

  DISALLOW_COPY_AND_ASSIGN(OverlayProcessor);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_H_
