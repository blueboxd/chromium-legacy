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
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

GraphicsContext* SVGFilterRecordingContext::BeginContent() {
  // Create a new context so the contents of the filter can be drawn and cached.
  paint_controller_ = std::make_unique<PaintController>();
  context_ = std::make_unique<GraphicsContext>(*paint_controller_);

  // Use initial_context_'s current paint chunk properties so that any new
  // chunk created during painting the content will be in the correct state.
  paint_controller_->UpdateCurrentPaintChunkProperties(
      nullptr,
      initial_context_.GetPaintController().CurrentPaintChunkProperties());

  return context_.get();
}

sk_sp<PaintRecord> SVGFilterRecordingContext::EndContent() {
  // Use the context that contains the filtered content.
  DCHECK(paint_controller_);
  DCHECK(context_);
  paint_controller_->CommitNewDisplayItems();
  sk_sp<PaintRecord> content =
      paint_controller_->GetPaintArtifact().GetPaintRecord(
          initial_context_.GetPaintController().CurrentPaintChunkProperties());

  // Content is cached by the source graphic so temporaries can be freed.
  paint_controller_ = nullptr;
  context_ = nullptr;
  return content;
}

void SVGFilterRecordingContext::Abort() {
  if (!paint_controller_)
    return;
  EndContent();
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

GraphicsContext* SVGFilterPainter::PrepareEffect(
    const LayoutObject& object,
    SVGFilterRecordingContext& recording_context) {
  filter_.ClearInvalidationMask();

  SVGElementResourceClient* client = SVGResources::GetClient(object);
  if (FilterData* filter_data = client->GetFilterData()) {
    // If the filterData already exists we do not need to record the content
    // to be filtered. This can occur if the content was previously recorded
    // or we are in a cycle.
    filter_data->UpdateStateOnPrepare();
    return nullptr;
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
  return recording_context.BeginContent();
}

void SVGFilterPainter::FinishEffect(
    const LayoutObject& object,
    const DisplayItemClient& display_item_client,
    SVGFilterRecordingContext& recording_context) {
  SVGElementResourceClient* client = SVGResources::GetClient(object);
  FilterData* filter_data = client->GetFilterData();
  if (!filter_data) {
    // Our state was torn down while we were being painted (selection style for
    // <text> can have this effect), or it was never created (invalid filter.)
    // In the former case we may have been in the process of recording content,
    // so make sure we put recording state into a consistent state.
    recording_context.Abort();
    return;
  }

  if (!filter_data->UpdateStateOnFinish())
    return;

  // Check for RecordingContent here because we may can be re-painting
  // without re-recording the contents to be filtered.
  if (filter_data->ContentNeedsUpdate())
    filter_data->UpdateContent(recording_context.EndContent());

  PaintFilteredContent(recording_context.PaintingContext(), object,
                       display_item_client, filter_data);
}

}  // namespace blink
