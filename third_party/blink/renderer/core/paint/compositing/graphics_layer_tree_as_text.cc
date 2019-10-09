// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/compositing/graphics_layer_tree_as_text.h"

#include "cc/layers/picture_layer.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/platform/geometry/geometry_as_json.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/logging_canvas.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"

namespace blink {

namespace {

typedef HashMap<int, int> RenderingContextMap;

String PointerAsString(const void* ptr) {
  WTF::TextStream ts;
  ts << ptr;
  return ts.Release();
}

FloatPoint ScrollPosition(const GraphicsLayer& layer) {
  if (const auto* scrollable_area =
          layer.Client().GetScrollableAreaForTesting(&layer)) {
    return scrollable_area->ScrollPosition();
  }
  return FloatPoint();
}

// For kOutputAsLayerTree only.
void AddTransformJSONProperties(const GraphicsLayer* layer, JSONObject& json) {
  const TransformationMatrix& transform = layer->Transform();
  if (!transform.IsIdentity())
    json.SetArray("transform", TransformAsJSONArray(transform));

  if (!transform.IsIdentityOrTranslation()) {
    json.SetArray("origin",
                  PointAsJSONArray(FloatPoint3D(layer->TransformOrigin())));
  }
}

std::unique_ptr<JSONObject> GraphicsLayerAsJSON(
    const GraphicsLayer* layer,
    LayerTreeFlags flags,
    const FloatPoint& position) {
  auto json = std::make_unique<JSONObject>();

  if (flags & kLayerTreeIncludesDebugInfo) {
    json->SetString("this", PointerAsString(layer));
    json->SetInteger("ccLayerId", layer->CcLayer()->id());
    if (layer->HasContentsLayer())
      json->SetInteger("ccContentsLayerId", layer->ContentsLayer()->id());
  }

  json->SetString("name", layer->DebugName());

  if (position != FloatPoint())
    json->SetArray("position", PointAsJSONArray(position));

  if (flags & kLayerTreeIncludesDebugInfo &&
      layer->OffsetFromLayoutObject() != IntSize()) {
    json->SetArray("offsetFromLayoutObject",
                   SizeAsJSONArray(layer->OffsetFromLayoutObject()));
  }

  // This is testing against gfx::Size(), *not* whether the size is empty.
  if (layer->Size() != gfx::Size())
    json->SetArray("bounds", SizeAsJSONArray(IntSize(layer->Size())));

  if (layer->ContentsOpaque())
    json->SetBoolean("contentsOpaque", true);

  if (!layer->DrawsContent())
    json->SetBoolean("drawsContent", false);

  if (!layer->ContentsAreVisible())
    json->SetBoolean("contentsVisible", false);

  if (!layer->BackfaceVisibility())
    json->SetString("backfaceVisibility", "hidden");

  if (flags & kLayerTreeIncludesDebugInfo)
    json->SetString("client", PointerAsString(&layer->Client()));

  if (Color(layer->BackgroundColor()).Alpha()) {
    json->SetString("backgroundColor",
                    Color(layer->BackgroundColor()).NameForLayoutTreeAsText());
  }

  if (flags & kOutputAsLayerTree)
    AddTransformJSONProperties(layer, *json);

  FloatPoint scroll_position(ScrollPosition(*layer));
  if (scroll_position != FloatPoint())
    json->SetArray("scrollPosition", PointAsJSONArray(scroll_position));

  if ((flags & kLayerTreeIncludesPaintInvalidations) &&
      layer->Client().IsTrackingRasterInvalidations() &&
      layer->GetRasterInvalidationTracking()) {
    layer->GetRasterInvalidationTracking()->AsJSON(json.get());
  }

  GraphicsLayerPaintingPhase painting_phase = layer->PaintingPhase();
  if ((flags & kLayerTreeIncludesPaintingPhases) && painting_phase) {
    auto painting_phases_json = std::make_unique<JSONArray>();
    if (painting_phase & kGraphicsLayerPaintBackground)
      painting_phases_json->PushString("GraphicsLayerPaintBackground");
    if (painting_phase & kGraphicsLayerPaintForeground)
      painting_phases_json->PushString("GraphicsLayerPaintForeground");
    if (painting_phase & kGraphicsLayerPaintMask)
      painting_phases_json->PushString("GraphicsLayerPaintMask");
    if (painting_phase & kGraphicsLayerPaintOverflowContents)
      painting_phases_json->PushString("GraphicsLayerPaintOverflowContents");
    if (painting_phase & kGraphicsLayerPaintCompositedScroll)
      painting_phases_json->PushString("GraphicsLayerPaintCompositedScroll");
    if (painting_phase & kGraphicsLayerPaintDecoration)
      painting_phases_json->PushString("GraphicsLayerPaintDecoration");
    json->SetArray("paintingPhases", std::move(painting_phases_json));
  }

  if (flags &
      (kLayerTreeIncludesDebugInfo | kLayerTreeIncludesCompositingReasons)) {
    bool debug = flags & kLayerTreeIncludesDebugInfo;
    {
      auto compositing_reasons_json = std::make_unique<JSONArray>();
      CompositingReasons compositing_reasons = layer->GetCompositingReasons();
      auto names = debug ? CompositingReason::Descriptions(compositing_reasons)
                         : CompositingReason::ShortNames(compositing_reasons);
      for (const char* name : names)
        compositing_reasons_json->PushString(name);
      json->SetArray("compositingReasons", std::move(compositing_reasons_json));
    }
    {
      auto squashing_disallowed_reasons_json = std::make_unique<JSONArray>();
      SquashingDisallowedReasons squashing_disallowed_reasons =
          layer->GetSquashingDisallowedReasons();
      auto names = debug ? SquashingDisallowedReason::Descriptions(
                               squashing_disallowed_reasons)
                         : SquashingDisallowedReason::ShortNames(
                               squashing_disallowed_reasons);
      for (const char* name : names)
        squashing_disallowed_reasons_json->PushString(name);
      json->SetArray("squashingDisallowedReasons",
                     std::move(squashing_disallowed_reasons_json));
    }
  }

  if (layer->MaskLayer()) {
    auto mask_layer_json = std::make_unique<JSONArray>();
    mask_layer_json->PushObject(
        GraphicsLayerAsJSON(layer->MaskLayer(), flags,
                            FloatPoint(layer->MaskLayer()->GetPosition())));
    json->SetArray("maskLayer", std::move(mask_layer_json));
  }

  if (layer->HasLayerState() && (flags & (kLayerTreeIncludesDebugInfo |
                                          kLayerTreeIncludesPaintRecords))) {
    json->SetString("layerState", layer->GetPropertyTreeState().ToString());
    json->SetValue("layerOffset",
                   PointAsJSONArray(layer->GetOffsetFromTransformNode()));
  }

#if DCHECK_IS_ON()
  if (layer->HasLayerState() && layer->DrawsContent() &&
      (flags & kLayerTreeIncludesPaintRecords))
    json->SetValue("paintRecord", RecordAsJSON(*layer->CapturePaintRecord()));
#endif

  return json;
}

class LayersAsJSONArray {
 public:
  LayersAsJSONArray(LayerTreeFlags flags)
      : flags_(flags),
        next_transform_id_(1),
        layers_json_(std::make_unique<JSONArray>()),
        transforms_json_(std::make_unique<JSONArray>()) {}

  // Outputs the layer tree rooted at |layer| as a JSON array, in paint order,
  // and the transform tree also as a JSON array.
  std::unique_ptr<JSONObject> operator()(const GraphicsLayer& layer) {
    auto json = std::make_unique<JSONObject>();
    Walk(layer);
    json->SetArray("layers", std::move(layers_json_));
    if (transforms_json_->size())
      json->SetArray("transforms", std::move(transforms_json_));
    return json;
  }

  int AddTransformJSON(const TransformPaintPropertyNode& transform) {
    auto it = transform_id_map_.find(&transform);
    if (it != transform_id_map_.end())
      return it->value;

    int parent_id = 0;
    if (transform.Parent())
      parent_id = AddTransformJSON(*transform.Parent());
    if (transform.IsIdentity() && !transform.RenderingContextId()) {
      transform_id_map_.Set(&transform, parent_id);
      return parent_id;
    }

    auto transform_json = std::make_unique<JSONObject>();
    int id = next_transform_id_++;
    transform_json->SetInteger("id", id);
    if (parent_id)
      transform_json->SetInteger("parent", parent_id);

    if (!transform.IsIdentity()) {
      transform_json->SetArray("transform",
                               TransformAsJSONArray(transform.SlowMatrix()));
    }

    if (!transform.IsIdentityOr2DTranslation() &&
        !transform.Matrix().IsIdentityOrTranslation())
      transform_json->SetArray("origin", PointAsJSONArray(transform.Origin()));

    if (!transform.FlattensInheritedTransform())
      transform_json->SetBoolean("flattenInheritedTransform", false);

    if (auto rendering_context = transform.RenderingContextId()) {
      auto context_lookup_result =
          rendering_context_map_.find(rendering_context);
      int rendering_id = rendering_context_map_.size() + 1;
      if (context_lookup_result == rendering_context_map_.end())
        rendering_context_map_.Set(rendering_context, rendering_id);
      else
        rendering_id = context_lookup_result->value;

      transform_json->SetInteger("renderingContext", rendering_id);
    }

    transforms_json_->PushObject(std::move(transform_json));
    return id;
  }

  void AddLayer(const GraphicsLayer& layer) {
    if (!layer.DrawsContent() && !(flags_ & kLayerTreeIncludesRootLayer))
      return;

    FloatPoint offset;
    if (layer.HasLayerState())
      offset = FloatPoint(layer.GetOffsetFromTransformNode());
    auto json = GraphicsLayerAsJSON(&layer, flags_, offset);
    if (layer.HasLayerState()) {
      int transform_id =
          AddTransformJSON(layer.GetPropertyTreeState().Transform());
      if (transform_id)
        json->SetInteger("transform", transform_id);
    }
    layers_json_->PushObject(std::move(json));
  }

  void Walk(const GraphicsLayer& layer) {
    AddLayer(layer);
    for (auto* const child : layer.Children())
      Walk(*child);
  }

 private:
  LayerTreeFlags flags_;
  int next_transform_id_;
  RenderingContextMap rendering_context_map_;
  std::unique_ptr<JSONArray> layers_json_;
  HashMap<const TransformPaintPropertyNode*, int> transform_id_map_;
  std::unique_ptr<JSONArray> transforms_json_;
};

}  // namespace

std::unique_ptr<JSONObject> GraphicsLayerTreeAsJSON(const GraphicsLayer* layer,
                                                    LayerTreeFlags flags) {
  if (flags & kOutputAsLayerTree) {
    std::unique_ptr<JSONObject> json =
        GraphicsLayerAsJSON(layer, flags, FloatPoint(layer->GetPosition()));
    if (layer->Children().size()) {
      auto children_json = std::make_unique<JSONArray>();
      for (wtf_size_t i = 0; i < layer->Children().size(); i++) {
        children_json->PushObject(
            GraphicsLayerTreeAsJSON(layer->Children()[i], flags));
      }
      json->SetArray("children", std::move(children_json));
    }
    return json;
  }

  return LayersAsJSONArray(flags)(*layer);
}

String GraphicsLayerTreeAsTextForTesting(const GraphicsLayer* layer,
                                         LayerTreeFlags flags) {
  return GraphicsLayerTreeAsJSON(layer, flags)->ToPrettyJSONString();
}

#if DCHECK_IS_ON()
void VerboseLogGraphicsLayerTree(const GraphicsLayer* root) {
  if (!VLOG_IS_ON(2))
    return;

  using GraphicsLayerTreeMap = HashMap<const GraphicsLayer*, String>;
  DEFINE_STATIC_LOCAL(GraphicsLayerTreeMap, s_previous_trees, ());
  LayerTreeFlags flags = VLOG_IS_ON(3) ? 0xffffffff : kOutputAsLayerTree;
  String new_tree = GraphicsLayerTreeAsTextForTesting(root, flags);
  auto it = s_previous_trees.find(root);
  if (it == s_previous_trees.end() || it->value != new_tree) {
    VLOG(2) << "GraphicsLayer tree:\n" << new_tree.Utf8();
    s_previous_trees.Set(root, new_tree);
    // For simplification, we don't remove deleted GraphicsLayers from the map.
  }
}
#endif

}  // namespace blink

#if DCHECK_IS_ON()
void showGraphicsLayerTree(const blink::GraphicsLayer* layer) {
  if (!layer) {
    LOG(ERROR) << "Cannot showGraphicsLayerTree for (nil).";
    return;
  }

  String output = blink::GraphicsLayerTreeAsTextForTesting(layer, 0xffffffff);
  LOG(ERROR) << output.Utf8();
}

void showGraphicsLayers(const blink::GraphicsLayer* layer) {
  if (!layer) {
    LOG(ERROR) << "Cannot showGraphicsLayers for (nil).";
    return;
  }

  String output = blink::GraphicsLayerTreeAsTextForTesting(
      layer, 0xffffffff & ~blink::kOutputAsLayerTree);
  LOG(ERROR) << output.Utf8();
}
#endif
