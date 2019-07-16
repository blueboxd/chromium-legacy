// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/compositing/content_layer_client_impl.h"

#include <memory>
#include "base/bind.h"
#include "base/optional.h"
#include "base/trace_event/traced_value.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_op_buffer.h"
#include "third_party/blink/renderer/platform/geometry/geometry_as_json.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_chunks_to_cc_layer.h"
#include "third_party/blink/renderer/platform/graphics/logging_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/display_item_list.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk_subset.h"
#include "third_party/blink/renderer/platform/graphics/paint/raster_invalidation_tracking.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

ContentLayerClientImpl::ContentLayerClientImpl()
    : cc_picture_layer_(cc::PictureLayer::Create(this)),
      raster_invalidator_(
          base::BindRepeating(&ContentLayerClientImpl::InvalidateRect,
                              base::Unretained(this))),
      layer_state_(PropertyTreeState::Uninitialized()) {
  cc_picture_layer_->SetLayerClient(weak_ptr_factory_.GetWeakPtr());
}

ContentLayerClientImpl::~ContentLayerClientImpl() {
  cc_picture_layer_->ClearClient();
}

static int GetTransformId(const TransformPaintPropertyNode* transform,
                          ContentLayerClientImpl::LayerAsJSONContext& context) {
  if (!transform)
    return 0;

  auto transform_lookup_result = context.transform_id_map.find(transform);
  if (transform_lookup_result != context.transform_id_map.end())
    return transform_lookup_result->value;

  int parent_id = GetTransformId(transform->Parent(), context);
  if (transform->IsIdentity() && !transform->RenderingContextId()) {
    context.transform_id_map.Set(transform, parent_id);
    return parent_id;
  }

  int transform_id = context.next_transform_id++;
  context.transform_id_map.Set(transform, transform_id);

  auto json = std::make_unique<JSONObject>();
  json->SetInteger("id", transform_id);
  if (parent_id)
    json->SetInteger("parent", parent_id);

  if (!transform->IsIdentity())
    json->SetArray("transform", TransformAsJSONArray(transform->SlowMatrix()));

  if (!transform->IsIdentityOr2DTranslation() &&
      !transform->Matrix().IsIdentityOrTranslation())
    json->SetArray("origin", PointAsJSONArray(transform->Origin()));

  if (!transform->FlattensInheritedTransform())
    json->SetBoolean("flattenInheritedTransform", false);

  if (auto rendering_context = transform->RenderingContextId()) {
    auto context_lookup_result =
        context.rendering_context_map.find(rendering_context);
    int rendering_id = context.rendering_context_map.size() + 1;
    if (context_lookup_result == context.rendering_context_map.end())
      context.rendering_context_map.Set(rendering_context, rendering_id);
    else
      rendering_id = context_lookup_result->value;

    json->SetInteger("renderingContext", rendering_id);
  }

  if (!context.transforms_json)
    context.transforms_json = std::make_unique<JSONArray>();
  context.transforms_json->PushObject(std::move(json));

  return transform_id;
}

// This is the CAP version of GraphicsLayer::LayerAsJSONInternal().
std::unique_ptr<JSONObject> ContentLayerClientImpl::LayerAsJSON(
    LayerAsJSONContext& context) const {
  auto json = std::make_unique<JSONObject>();
  json->SetString("name", debug_name_);

  if (context.flags & kLayerTreeIncludesDebugInfo)
    json->SetString("this", String::Format("%p", cc_picture_layer_.get()));

  FloatPoint position(cc_picture_layer_->offset_to_transform_parent().x(),
                      cc_picture_layer_->offset_to_transform_parent().y());
  if (position != FloatPoint())
    json->SetArray("position", PointAsJSONArray(position));

  IntSize bounds(cc_picture_layer_->bounds().width(),
                 cc_picture_layer_->bounds().height());
  if (!bounds.IsEmpty())
    json->SetArray("bounds", SizeAsJSONArray(bounds));

  if (cc_picture_layer_->contents_opaque())
    json->SetBoolean("contentsOpaque", true);

  if (!cc_picture_layer_->DrawsContent())
    json->SetBoolean("drawsContent", false);

  if (!cc_picture_layer_->double_sided())
    json->SetString("backfaceVisibility", "hidden");

  Color background_color(cc_picture_layer_->background_color());
  if (background_color.Alpha()) {
    json->SetString("backgroundColor",
                    background_color.NameForLayoutTreeAsText());
  }

#if DCHECK_IS_ON()
  if (context.flags & kLayerTreeIncludesDebugInfo)
    json->SetValue("paintChunkContents", paint_chunk_debug_data_->Clone());
#endif

  if ((context.flags & kLayerTreeIncludesPaintInvalidations) &&
      raster_invalidator_.GetTracking())
    raster_invalidator_.GetTracking()->AsJSON(json.get());

  if (int transform_id = GetTransformId(&layer_state_.Transform(), context))
    json->SetInteger("transform", transform_id);

#if DCHECK_IS_ON()
  if (context.flags & kLayerTreeIncludesPaintRecords) {
    LoggingCanvas canvas;
    cc_display_item_list_->Raster(&canvas);
    json->SetValue("paintRecord", canvas.Log());
  }
#endif

  return json;
}

std::unique_ptr<base::trace_event::TracedValue>
ContentLayerClientImpl::TakeDebugInfo(const cc::Layer* layer) {
  DCHECK_EQ(layer, cc_picture_layer_.get());
  auto traced_value = std::make_unique<base::trace_event::TracedValue>();
  traced_value->SetString("layer_name",
                          WTF::StringUTF8Adaptor(debug_name_).AsStringPiece());
  if (auto* tracking = raster_invalidator_.GetTracking()) {
    tracking->AddToTracedValue(*traced_value);
    tracking->ClearInvalidations();
  }
  // TODO(wangxianzhu): Do we need compositing_reasons,
  // squashing_disallowed_reasons and owner_node_id?
  return traced_value;
}

scoped_refptr<cc::PictureLayer> ContentLayerClientImpl::UpdateCcPictureLayer(
    scoped_refptr<const PaintArtifact> paint_artifact,
    const PaintChunkSubset& paint_chunks,
    const gfx::Rect& layer_bounds,
    const PropertyTreeState& layer_state) {
  if (paint_chunks[0].is_cacheable)
    id_.emplace(paint_chunks[0].id);
  else
    id_ = base::nullopt;

  // TODO(wangxianzhu): Avoid calling DebugName() in official release build.
  debug_name_ = paint_chunks[0].id.client.DebugName();
  const auto& display_item_list = paint_artifact->GetDisplayItemList();

#if DCHECK_IS_ON()
  paint_chunk_debug_data_ = std::make_unique<JSONArray>();
  for (const auto& chunk : paint_chunks) {
    auto json = std::make_unique<JSONObject>();
    json->SetString("data", chunk.ToString());
    json->SetArray("displayItems",
                   paint_artifact->GetDisplayItemList().SubsequenceAsJSON(
                       chunk.begin_index, chunk.end_index,
                       DisplayItemList::kShowOnlyDisplayItemTypes));
    paint_chunk_debug_data_->PushObject(std::move(json));
  }
#endif

  // The raster invalidator will only handle invalidations within a cc::Layer so
  // we need this invalidation if the layer's properties have changed.
  if (layer_state != layer_state_)
    cc_picture_layer_->SetSubtreePropertyChanged();

  raster_invalidator_.Generate(paint_artifact, paint_chunks, layer_bounds,
                               layer_state);
  layer_state_ = layer_state;

  // Note: cc::Layer API assumes the layer bounds start at (0, 0), but the
  // bounding box of a paint chunk does not necessarily start at (0, 0) (and
  // could even be negative). Internally the generated layer translates the
  // paint chunk to align the bounding box to (0, 0) and we set the layer's
  // offset_to_transform_parent with the origin of the paint chunk here.
  cc_picture_layer_->SetOffsetToTransformParent(
      layer_bounds.OffsetFromOrigin());
  cc_picture_layer_->SetBounds(layer_bounds.size());
  cc_picture_layer_->SetIsDrawable(true);
  cc_picture_layer_->SetHitTestable(true);

  base::Optional<RasterUnderInvalidationCheckingParams> params;
  if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled()) {
    params.emplace(*raster_invalidator_.GetTracking(),
                   IntRect(0, 0, layer_bounds.width(), layer_bounds.height()),
                   debug_name_);
  }
  cc_display_item_list_ = PaintChunksToCcLayer::Convert(
      paint_chunks, layer_state, layer_bounds.OffsetFromOrigin(),
      display_item_list, cc::DisplayItemList::kTopLevelDisplayItemList,
      base::OptionalOrNullptr(params));

  cc_picture_layer_->SetSafeOpaqueBackgroundColor(
      paint_chunks[0].safe_opaque_background_color);
  // TODO(masonfreed): We don't need to set the background color here; only the
  // safe opaque background color matters. But making that change would require
  // rebaselining 787 tests to remove the "background_color" property from the
  // layer dumps.
  cc_picture_layer_->SetBackgroundColor(
      paint_chunks[0].safe_opaque_background_color);
  return cc_picture_layer_;
}

}  // namespace blink
