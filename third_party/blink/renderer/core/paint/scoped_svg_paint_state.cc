/*
 * Copyright (C) 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/paint/scoped_svg_paint_state.h"

#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_filter.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources_cache.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"

namespace blink {

SVGFilterRecordingContext::SVGFilterRecordingContext(
    const PaintInfo& initial_paint_info)
    // Create a new controller and context so the contents of the filter can be
    // drawn and cached.
    : paint_controller_(std::make_unique<PaintController>()),
      context_(std::make_unique<GraphicsContext>(*paint_controller_)),
      paint_info_(*context_, initial_paint_info) {
  // Use initial_paint_info's current paint chunk properties so that any new
  // chunk created during painting the content will be in the correct state.
  paint_controller_->UpdateCurrentPaintChunkProperties(
      nullptr, initial_paint_info.context.GetPaintController()
                   .CurrentPaintChunkProperties());
  // Because we cache the filter contents and do not invalidate on paint
  // invalidation rect changes, we need to paint the entire filter region so
  // elements outside the initial paint (due to scrolling, etc) paint.
  paint_info_.ApplyInfiniteCullRect();
}

SVGFilterRecordingContext::~SVGFilterRecordingContext() = default;

sk_sp<PaintRecord> SVGFilterRecordingContext::GetPaintRecord(
    const PaintInfo& initial_paint_info) {
  paint_controller_->CommitNewDisplayItems();
  return paint_controller_->GetPaintArtifact().GetPaintRecord(
      initial_paint_info.context.GetPaintController()
          .CurrentPaintChunkProperties());
}

static void PaintFilteredContent(GraphicsContext& context,
                                 const LayoutObject& object,
                                 const DisplayItemClient& display_item_client,
                                 FilterData* filter_data) {
  if (DrawingRecorder::UseCachedDrawingIfPossible(context, display_item_client,
                                                  DisplayItem::kSVGFilter))
    return;

  DrawingRecorder recorder(context, display_item_client,
                           DisplayItem::kSVGFilter);
  sk_sp<PaintFilter> image_filter = filter_data->CreateFilter();
  context.Save();

  // Clip drawing of filtered image to the minimum required paint rect.
  const FloatRect object_bounds = object.StrokeBoundingBox();
  const FloatRect paint_rect = filter_data->MapRect(object_bounds);
  context.ClipRect(paint_rect);

  // Use the union of the pre-image and the post-image as the layer bounds.
  const FloatRect layer_bounds = UnionRect(object_bounds, paint_rect);
  context.BeginLayer(1, SkBlendMode::kSrcOver, &layer_bounds, kColorFilterNone,
                     std::move(image_filter));
  context.EndLayer();
  context.Restore();
}

ScopedSVGPaintState::~ScopedSVGPaintState() {
  if (filter_data_ && filter_data_->UpdateStateOnFinish()) {
    DCHECK(SVGResourcesCache::CachedResourcesForLayoutObject(object_));
    DCHECK(
        SVGResourcesCache::CachedResourcesForLayoutObject(object_)->Filter());
    if (filter_recording_context_) {
      filter_data_->UpdateContent(
          filter_recording_context_->GetPaintRecord(paint_info_));
      filter_recording_context_ = nullptr;
    }
    PaintFilteredContent(paint_info_.context, object_, display_item_client_,
                         filter_data_);
    filter_data_ = nullptr;
  }
}

bool ScopedSVGPaintState::ApplyEffects() {
#if DCHECK_IS_ON()
  DCHECK(!apply_clip_mask_and_filter_if_necessary_called_);
  apply_clip_mask_and_filter_if_necessary_called_ = true;
#endif
  ApplyPaintPropertyState();

  // When rendering clip paths as masks, only geometric operations should be
  // included so skip non-geometric operations such as compositing, masking, and
  // filtering.
  if (GetPaintInfo().IsRenderingClipPathAsMaskImage()) {
    DCHECK(!object_.IsSVGRoot());
    ApplyClipIfNecessary();
    return true;
  }

  // LayoutSVGRoot and LayoutSVGForeignObject always have a self-painting
  // PaintLayer (hence comments below about PaintLayerPainter).
  bool is_svg_root_or_foreign_object =
      object_.IsSVGRoot() || object_.IsSVGForeignObject();
  if (is_svg_root_or_foreign_object) {
    // PaintLayerPainter takes care of opacity and blend mode.
    DCHECK(object_.HasLayer() || !(object_.StyleRef().HasOpacity() ||
                                   object_.StyleRef().HasBlendMode() ||
                                   object_.StyleRef().ClipPath()));
  } else {
    ApplyClipIfNecessary();
  }

  SVGResources* resources =
      SVGResourcesCache::CachedResourcesForLayoutObject(object_);

  ApplyMaskIfNecessary(resources);

  if (is_svg_root_or_foreign_object) {
    // PaintLayerPainter takes care of filter.
    DCHECK(object_.HasLayer() || !object_.StyleRef().HasFilter());
  } else if (!ApplyFilterIfNecessary(resources)) {
    return false;
  }
  return true;
}

void ScopedSVGPaintState::ApplyPaintPropertyState() {
  // SVGRoot works like normal CSS replaced element and its effects are
  // applied as stacking context effect by PaintLayerPainter.
  if (object_.IsSVGRoot())
    return;

  const auto* fragment = GetPaintInfo().FragmentToPaint(object_);
  if (!fragment)
    return;
  const auto* properties = fragment->PaintProperties();
  // MaskClip() implies Effect(), thus we don't need to check MaskClip().
  if (!properties || (!properties->Effect() && !properties->ClipPathClip()))
    return;

  auto& paint_controller = GetPaintInfo().context.GetPaintController();
  PropertyTreeState state = paint_controller.CurrentPaintChunkProperties();
  if (const auto* effect = properties->Effect())
    state.SetEffect(*effect);
  if (const auto* mask_clip = properties->MaskClip())
    state.SetClip(*mask_clip);
  else if (const auto* clip_path_clip = properties->ClipPathClip())
    state.SetClip(*clip_path_clip);
  scoped_paint_chunk_properties_.emplace(
      paint_controller, state, display_item_client_,
      DisplayItem::PaintPhaseToSVGEffectType(GetPaintInfo().phase));
}

void ScopedSVGPaintState::ApplyClipIfNecessary() {
  if (object_.StyleRef().ClipPath()) {
    clip_path_clipper_.emplace(GetPaintInfo().context, object_,
                               display_item_client_, PhysicalOffset());
  }
}

void ScopedSVGPaintState::ApplyMaskIfNecessary(SVGResources* resources) {
  if (resources && resources->Masker())
    mask_painter_.emplace(paint_info_.context, object_, display_item_client_);
}

static bool HasReferenceFilterOnly(const ComputedStyle& style) {
  if (!style.HasFilter())
    return false;
  const FilterOperations& operations = style.Filter();
  if (operations.size() != 1)
    return false;
  return operations.at(0)->GetType() == FilterOperation::REFERENCE;
}

bool ScopedSVGPaintState::ApplyFilterIfNecessary(SVGResources* resources) {
  if (!resources)
    return !HasReferenceFilterOnly(object_.StyleRef());
  LayoutSVGResourceFilter* filter = resources->Filter();
  if (!filter)
    return true;
  filter->ClearInvalidationMask();
  filter_data_ = SVGResources::GetClient(object_)->UpdateFilterData();
  // If we have no filter data (== the filter was invalid) or if we
  // don't need to update the source graphics, we can short-circuit
  // here.
  if (!filter_data_ || !filter_data_->ContentNeedsUpdate())
    return false;
  // Because the filter needs to cache its contents we replace the context
  // during filtering with the filter's context.
  filter_recording_context_ =
      std::make_unique<SVGFilterRecordingContext>(paint_info_);
  return true;
}

}  // namespace blink
