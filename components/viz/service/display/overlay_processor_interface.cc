// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_interface.h"

#include "base/metrics/histogram_macros.h"
#if defined(OS_MACOSX) || defined(OS_WIN)
#include "components/viz/service/display/overlay_processor.h"
#else
#include "components/viz/service/display/overlay_candidate_validator_strategy.h"
#include "components/viz/service/display/overlay_processor_using_strategy.h"
#endif

namespace viz {
namespace {
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class UnderlayDamage {
  kZeroDamageRect,
  kNonOccludingDamageOnly,
  kOccludingDamageOnly,
  kOccludingAndNonOccludingDamages,
  kMaxValue = kOccludingAndNonOccludingDamages,
};
}  // namespace

// Record UMA histograms for overlays
// 1. Underlay vs. Overlay
// 2. Full screen mode vs. Non Full screen (Windows) mode
// 3. Overlay zero damage rect vs. non zero damage rect
// 4. Underlay zero damage rect, non-zero damage rect with non-occluding damage
//   only, non-zero damage rect with occluding damage, and non-zero damage rect
//   with both damages

// static
void OverlayProcessorInterface::RecordOverlayDamageRectHistograms(
    bool is_overlay,
    bool has_occluding_surface_damage,
    bool zero_damage_rect,
    bool occluding_damage_equal_to_damage_rect) {
  if (is_overlay) {
    UMA_HISTOGRAM_BOOLEAN("Viz.DisplayCompositor.RootDamageRect.Overlay",
                          !zero_damage_rect);
  } else {  // underlay
    UnderlayDamage underlay_damage = UnderlayDamage::kZeroDamageRect;
    if (zero_damage_rect) {
      underlay_damage = UnderlayDamage::kZeroDamageRect;
    } else {
      if (has_occluding_surface_damage) {
        if (occluding_damage_equal_to_damage_rect) {
          underlay_damage = UnderlayDamage::kOccludingDamageOnly;
        } else {
          underlay_damage = UnderlayDamage::kOccludingAndNonOccludingDamages;
        }
      } else {
        underlay_damage = UnderlayDamage::kNonOccludingDamageOnly;
      }
    }
    UMA_HISTOGRAM_ENUMERATION("Viz.DisplayCompositor.RootDamageRect.Underlay",
                              underlay_damage);
  }
}

std::unique_ptr<OverlayProcessorInterface>
OverlayProcessorInterface::CreateOverlayProcessor(
    SkiaOutputSurface* skia_output_surface,
    gpu::SurfaceHandle surface_handle,
    const OutputSurface::Capabilities& capabilities,
    const RendererSettings& renderer_settings) {
#if defined(OS_MACOSX) || defined(OS_WIN)
  auto validator = OverlayCandidateValidator::Create(
      surface_handle, capabilities, renderer_settings);
  auto processor = base::WrapUnique(new OverlayProcessor(std::move(validator)));
#else  // defined(USE_OZONE) || defined(OS_ANDROID) || Default
  auto validator = OverlayCandidateValidatorStrategy::Create(
      surface_handle, capabilities, renderer_settings);
  auto processor = base::WrapUnique(new OverlayProcessorUsingStrategy(
      skia_output_surface, std::move(validator)));
#endif

#if defined(OS_WIN)
  processor->InitializeDCOverlayProcessor(
      std::make_unique<DCLayerOverlayProcessor>(capabilities,
                                                renderer_settings));
#endif
  return processor;
}

OverlayProcessorInterface::OutputSurfaceOverlayPlane
OverlayProcessorInterface::ProcessOutputSurfaceAsOverlay(
    const gfx::Size& viewport_size,
    const gfx::BufferFormat& buffer_format,
    const gfx::ColorSpace& color_space,
    bool has_alpha) {
  OutputSurfaceOverlayPlane overlay_plane;
  overlay_plane.transform = gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE;
  overlay_plane.resource_size = viewport_size;
  overlay_plane.format = buffer_format;
  overlay_plane.color_space = color_space;
  overlay_plane.enable_blending = has_alpha;

  // Adjust transformation and display_rect based on display rotation.
  overlay_plane.display_rect =
      gfx::RectF(viewport_size.width(), viewport_size.height());

#if defined(ALWAYS_ENABLE_BLENDING_FOR_PRIMARY)
  // On Chromecast, always use RGBA as the scanout format for the primary plane.
  overlay_plane.enable_blending = true;
#endif
  return overlay_plane;
}

}  // namespace viz
