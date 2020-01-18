// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_INTERFACE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_INTERFACE_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/overlay_candidate.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/gpu_task_scheduler_helper.h"

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
class OutputSurface;
class RendererSettings;

// This class is called inside the DirectRenderer to separate the contents that
// should be send into the overlay system and the contents that requires
// compositing from the DirectRenderer. This class has different subclass
// implemented by different platforms. This class defines the minimal interface
// for overlay processing that each platform needs to implement.
class VIZ_SERVICE_EXPORT OverlayProcessorInterface {
 public:
#if defined(OS_MACOSX)
  using CandidateList = CALayerOverlayList;
#elif defined(OS_WIN)
  using CandidateList = DCLayerOverlayList;
#else
  // Default.
  using CandidateList = OverlayCandidateList;
#endif

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
    // TODO(weiliangc): Should be replaced by SharedImage mailbox.
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

  static std::unique_ptr<OverlayProcessorInterface> CreateOverlayProcessor(
      SkiaOutputSurface* skia_output_surface,
      gpu::SurfaceHandle surface_handle,
      const OutputSurface::Capabilities& capabilities,
      const RendererSettings& renderer_settings,
      gpu::SharedImageManager* shared_image_manager,
      scoped_refptr<gpu::GpuTaskSchedulerHelper> gpu_task_scheduler,
      gpu::SharedImageInterface* shared_image_interface);

  virtual ~OverlayProcessorInterface() {}

  virtual bool IsOverlaySupported() const = 0;
  virtual gfx::Rect GetAndResetOverlayDamage() = 0;

  // Returns true if the platform supports hw overlays and surface occluding
  // damage rect needs to be computed since it will be used by overlay
  // processor.
  virtual bool NeedsSurfaceOccludingDamageRect() const = 0;

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
      std::vector<gfx::Rect>* content_bounds) = 0;

  // For Mac, if we successfully generated a candidate list for CALayerOverlay,
  // we no longer need the |output_surface_plane|. This function takes a pointer
  // to the base::Optional instance so the instance can be reset.
  // TODO(weiliangc): Internalize the |output_surface_plane| inside the overlay
  // processor.
  virtual void AdjustOutputSurfaceOverlay(
      base::Optional<OutputSurfaceOverlayPlane>* output_surface_plane) = 0;

  // TODO(weiliangc): Make it pure virtual after it is implemented by every
  // subclass.
  virtual void ScheduleOverlays(
      DisplayResourceProvider* display_resource_provider);

  // These two functions are used by Android SurfaceControl.
  virtual void SetDisplayTransformHint(gfx::OverlayTransform transform) {}
  virtual void SetViewportSize(const gfx::Size& size) {}

 protected:
  OverlayProcessorInterface() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(OverlayProcessorInterface);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_OVERLAY_PROCESSOR_INTERFACE_H_
