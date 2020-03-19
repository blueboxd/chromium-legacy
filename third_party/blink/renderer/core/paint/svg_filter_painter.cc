// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/svg_filter_painter.h"

#include <utility>

#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_filter.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/paint/filter_effect_builder.h"
#include "third_party/blink/renderer/core/svg/graphics/filters/svg_filter_builder.h"
#include "third_party/blink/renderer/core/svg/svg_filter_element.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter.h"
#include "third_party/blink/renderer/platform/graphics/filters/source_graphic.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"

namespace blink {

SVGFilterRecordingContext::SVGFilterRecordingContext(
    const PaintInfo& initial_paint_info)
    // Create a new context so the contents of the filter can be drawn and
    // cached.
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

FilterData* SVGFilterPainter::PrepareEffect(const LayoutObject& object) {
  SVGElementResourceClient* client = SVGResources::GetClient(object);
  if (FilterData* filter_data = client->GetFilterData()) {
    // If the filterData already exists we do not need to record the content
    // to be filtered. This can occur if the content was previously recorded
    // or we are in a cycle.
    filter_data->UpdateStateOnPrepare();
    return filter_data;
  }

  auto* node_map = MakeGarbageCollected<SVGFilterGraphNodeMap>();
  FilterEffectBuilder builder(SVGResources::ReferenceBoxForEffects(object), 1);
  Filter* filter = builder.BuildReferenceFilter(
      To<SVGFilterElement>(*filter_.GetElement()), nullptr, node_map);
  if (!filter || !filter->LastEffect())
    return nullptr;

  IntRect source_region = EnclosingIntRect(object.StrokeBoundingBox());
  filter->GetSourceGraphic()->SetSourceRect(source_region);

  auto* filter_data =
      MakeGarbageCollected<FilterData>(filter->LastEffect(), node_map);
  // TODO(pdr): Can this be moved out of painter?
  client->SetFilterData(filter_data);
  return filter_data;
}

}  // namespace blink
