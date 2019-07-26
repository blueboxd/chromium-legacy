// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "components/viz/common/display/overlay_strategy.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/ipc/common/surface_handle.h"

#if defined(OS_WIN)
#include "components/viz/service/display/dc_layer_overlay.h"
#endif

#if defined(OS_MACOSX)
#include "components/viz/service/display/ca_layer_overlay.h"
#endif

namespace cc {
class DisplayResourceProvider;
}

namespace viz {
class OverlayCandidateValidator;
class RendererSettings;
class ContextProvider;

class VIZ_SERVICE_EXPORT OverlayProcessor {
 public:
#if defined(OS_ANDROID)
  using CandidateList = OverlayCandidateList;
#elif defined(OS_MACOSX)
  using CandidateList = CALayerOverlayList;
#elif defined(OS_WIN)
  using CandidateList = DCLayerOverlayList;
#elif defined(USE_OZONE)
  using CandidateList = OverlayCandidateList;
#else
  // Default.
  using CandidateList = OverlayCandidateList;
#endif

  using FilterOperationsMap =
      base::flat_map<RenderPassId, cc::FilterOperations*>;

  static void RecordOverlayDamageRectHistograms(
      bool is_overlay,
      bool has_occluding_surface_damage,
      bool zero_damage_rect,
      bool occluding_damage_equal_to_damage_rect);

  // Data needed to represent output surface as overlay plane. Due to default
  // values for primary plane, this is a partial list of OverlayCandidate.
  struct VIZ_SERVICE_EXPORT OutputSurfaceOverlayPlane {
    // Display's rotation information.
    gfx::OverlayTransform transform;
    // Rect on the display to position to. This takes in account of Display's
    // rotation.
    gfx::RectF display_rect;
    // Size of output surface in pixels.
    gfx::Size resource_size;
    // Format of the buffer to scanout.
    gfx::BufferFormat format;
    // ColorSpace of the buffer for scanout.
    gfx::ColorSpace color_space;
    // Enable blending when we have underlay.
    bool enable_blending;
    // Gpu fence to wait for before overlay is ready for display.
    unsigned gpu_fence_id;
  };

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

    virtual void AdjustOutputSurfaceOverlay(
        OutputSurfaceOverlayPlane* output_surface_plane) {}

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
      CandidateList* overlay_candidates,
      gfx::Rect* damage_rect,
      std::vector<gfx::Rect>* content_bounds);

  // TODO(weiliangc): Eventually the asymmetry between primary plane and
  // non-primary places should be internalized and should not have a special
  // API.
  virtual base::Optional<OutputSurfaceOverlayPlane>
  ProcessOutputSurfaceAsOverlay(const gfx::Size& viewport_size,
                                const gfx::BufferFormat& buffer_format,
                                const gfx::ColorSpace& color_space) const;

#if defined(OS_WIN)
  void SetDCHasHwOverlaySupportForTesting() {
    dc_processor_->SetHasHwOverlaySupport();
  }
#endif

 protected:
  explicit OverlayProcessor(const ContextProvider* context_provider);
  void SetOverlayCandidateValidator(
      std::unique_ptr<OverlayCandidateValidator> overlay_validator);

  virtual bool ProcessForOverlayInternal(
      DisplayResourceProvider* resource_provider,
      RenderPassList* render_passes,
      const FilterOperationsMap& render_pass_filters,
      const FilterOperationsMap& render_pass_backdrop_filters,
      CandidateList* overlay_candidates,
      gfx::Rect* damage_rect);

  StrategyList strategies_;
  std::unique_ptr<OverlayCandidateValidator> overlay_validator_;
  gfx::Rect overlay_damage_rect_;
  gfx::Rect previous_frame_underlay_rect_;
  bool previous_frame_underlay_was_unoccluded_ = false;

 private:
  bool ProcessForDCLayers(
      DisplayResourceProvider* resource_provider,
      RenderPassList* render_passes,
      const FilterOperationsMap& render_pass_filters,
      const FilterOperationsMap& render_pass_backdrop_filters,
      CandidateList* overlay_candidates,
      gfx::Rect* damage_rect);
  // Update |damage_rect| by removing damage casued by |candidates|.
  void UpdateDamageRect(OverlayCandidateList* candidates,
                        const gfx::Rect& previous_frame_underlay_rect,
                        bool previous_frame_underlay_was_unoccluded,
                        const QuadList* quad_list,
                        gfx::Rect* damage_rect);

#if defined(OS_WIN)
  std::unique_ptr<DCLayerOverlayProcessor> dc_processor_;
#endif

  DISALLOW_COPY_AND_ASSIGN(OverlayProcessor);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_H_
