// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/graphics_layer_display_item.h"

#include <utility>

#include "cc/layers/layer.h"
#include "cc/layers/picture_layer.h"
#include "third_party/blink/renderer/platform/graphics/compositing/layers_as_json.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

GraphicsLayerDisplayItem::GraphicsLayerDisplayItem(
    const GraphicsLayer& graphics_layer)
    : DisplayItem(graphics_layer, kGraphicsLayerWrapper, sizeof(*this)),
      graphics_layer_(graphics_layer) {}

bool GraphicsLayerDisplayItem::Equals(const DisplayItem& other) const {
  return GetType() == other.GetType() &&
         GetGraphicsLayer() ==
             static_cast<const GraphicsLayerDisplayItem&>(other)
                 .GetGraphicsLayer();
}

#if DCHECK_IS_ON()
void GraphicsLayerDisplayItem::PropertiesAsJSON(JSONObject& json) const {
  DisplayItem::PropertiesAsJSON(json);
  json.SetInteger("layer", graphics_layer_.CcLayer()->id());
  FloatPoint offset(graphics_layer_.GetOffsetFromTransformNode());
  json.SetDouble("offset_x", offset.X());
  json.SetDouble("offset_y", offset.Y());
}
#endif

void RecordGraphicsLayer(GraphicsContext& context,
                         const GraphicsLayer& graphics_layer) {
  // In pre-CompositeAfterPaint, the GraphicsLayer hierarchy is still built
  // during CompositingUpdate, and we have to clear them here to ensure no
  // extraneous layers are still attached. In future we will disable all
  // those layer hierarchy code so we won't need this line.
  DCHECK(!RuntimeEnabledFeatures::CompositeAfterPaintEnabled());
  graphics_layer.CcLayer()->RemoveAllChildren();

  PaintController& paint_controller = context.GetPaintController();

  // This is like ScopedPaintChunkProperties but uses null id because graphics
  // layer chunk doesn't need an id nor a client.
  const PropertyTreeState& properties = graphics_layer.GetPropertyTreeState();
  PropertyTreeState previous_properties(
      paint_controller.CurrentPaintChunkProperties());
  paint_controller.UpdateCurrentPaintChunkProperties(nullptr, properties);
  paint_controller.CreateAndAppend<GraphicsLayerDisplayItem>(graphics_layer);
  paint_controller.UpdateCurrentPaintChunkProperties(nullptr,
                                                     previous_properties);
}

}  // namespace blink
