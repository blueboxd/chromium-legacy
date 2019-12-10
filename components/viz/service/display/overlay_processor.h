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
class OverlayCandidateList;
class OverlayCandidateValidator;
class RendererSettings;

// This class is called inside the DirectRenderer to separate the contents that
// should be send into the overlay system and the contents that requires
// compositing from the DirectRenderer. This class has different subclass
// implemented by different platforms.
class VIZ_SERVICE_EXPORT OverlayProcessor {
 public:
#if defined(OS_MACOSX)
  using CandidateList = CALayerOverlayList;
#elif defined(OS_WIN)
  using CandidateList = DCLayerOverlayList;
#else
  // Default.
  using CandidateList = OverlayCandidateList;
#endif

  using OverlayValidator = OverlayCandidateValidator;

  using FilterOperationsMap =
      base::flat_map<RenderPassId, cc::FilterOperations*>;

  // Used by Window's DCLayerOverlay system and OverlayProcessorUsingStrategy.
  static void RecordOverlayDamageRectHistograms(
      bool is_overlay,
      bool has_occluding_surface_damage,
      bool zero_damage_rect,
      bool occluding_damage_equal_to_damage_rect);

  // Data needed to represent |OutputSurface| as an overlay plane. Due to the
  // default values for the primary plane, this is a partial list of
  // OverlayCandidate.
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

  // TODO(weiliangc): Eventually the asymmetry between primary plane and
  // non-primary places should be internalized and should not have a special
  // API.
  static OutputSurfaceOverlayPlane ProcessOutputSurfaceAsOverlay(
      const gfx::Size& viewport_size,
      const gfx::BufferFormat& buffer_format,
      const gfx::ColorSpace& color_space,
      bool has_alpha);

  static std::unique_ptr<OverlayProcessor> CreateOverlayProcessor(
      SkiaOutputSurface* skia_output_surface,
      gpu::SurfaceHandle surface_handle,
      const OutputSurface::Capabilities& capabilities,
      const RendererSettings& renderer_settings);

  virtual ~OverlayProcessor();

  virtual bool IsOverlaySupported() const;

  virtual gfx::Rect GetAndResetOverlayDamage();

  // Returns true if the platform supports hw overlays and surface occluding
  // damage rect needs to be computed since it will be used by overlay
  // processor.
  virtual bool NeedsSurfaceOccludingDamageRect() const;

  // Attempt to replace quads from the specified root render pass with overlays
  // or CALayers. This must be called every frame.
  virtual void ProcessForOverlays(
      DisplayResourceProvider* resource_provider,
      RenderPassList* render_passes,
      const SkMatrix44& output_color_matrix,
      const FilterOperationsMap& render_pass_filters,
      const FilterOperationsMap& render_pass_backdrop_filters,
      OutputSurfaceOverlayPlane* output_surface_plane,
      CandidateList* overlay_candidates,
      gfx::Rect* damage_rect,
      std::vector<gfx::Rect>* content_bounds);

  // For Mac, if we successfully generated a candidate list for CALayerOverlay,
  // we no longer need the |output_surface_plane|. This function takes a pointer
  // to the base::Optional instance so the instance can be reset. This is also
  // used to remove |output_surface_plane| when the successful overlay strategy
  // doesn't need the |output_surface_plane|. SurfaceControl also overrides this
  // function to adjust the rotation.
  // TODO(weiliangc): Internalize the |output_surface_plane| inside the overlay
  // processor.
  virtual void AdjustOutputSurfaceOverlay(
      base::Optional<OutputSurfaceOverlayPlane>* output_surface_plane);

  // These two functions are used by Android SurfaceControl.
  virtual void SetDisplayTransformHint(gfx::OverlayTransform transform) {}
  virtual void SetValidatorViewportSize(const gfx::Size& size) {}
  // TODO(weiliangc): Delete this function once software mirror (non-OOPD) is
  // deleted.
  virtual void SetSoftwareMirrorMode(bool software_mirror_mode) {}

 protected:
  explicit OverlayProcessor(
      std::unique_ptr<OverlayValidator> overlay_validator);

  // For testing.
  const OverlayValidator* GetOverlayCandidateValidator() const {
    return overlay_validator_.get();
  }
  std::unique_ptr<OverlayValidator> overlay_validator_;

 private:
#if defined(OS_WIN)
  void InitializeDCOverlayProcessor(
      std::unique_ptr<DCLayerOverlayProcessor> dc_layer_overlay_processor);
#endif

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
