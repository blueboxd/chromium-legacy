// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/foreign_layer_display_item.h"

#include <utility>

#include "cc/layers/layer.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

namespace {

class ForeignLayerDisplayItemClient final : public DisplayItemClient {
 public:
  ForeignLayerDisplayItemClient(scoped_refptr<cc::Layer> layer,
                                const FloatPoint& offset)
      : layer_(std::move(layer)), offset_(offset) {
    Invalidate(PaintInvalidationReason::kUncacheable);
  }

  String DebugName() const final {
#if DCHECK_IS_ON()
    return String("ForeignLayer for ") + layer_->DebugName().c_str();
#else
    return "ForeignLayer";
#endif
  }

  DOMNodeId OwnerNodeId() const final { return layer_->owner_node_id(); }

  IntRect VisualRect() const final {
    const auto& bounds = layer_->bounds();
    return EnclosingIntRect(
        FloatRect(offset_.X(), offset_.Y(), bounds.width(), bounds.height()));
  }

  cc::Layer* GetLayer() const { return layer_.get(); }

 private:
  scoped_refptr<cc::Layer> layer_;
  FloatPoint offset_;
};

}  // anonymous namespace

ForeignLayerDisplayItem::ForeignLayerDisplayItem(Type type,
                                                 scoped_refptr<cc::Layer> layer,
                                                 const FloatPoint& offset)
    : DisplayItem(*new ForeignLayerDisplayItemClient(std::move(layer), offset),
                  type,
                  sizeof(*this)),
      offset_(offset) {
  DCHECK(IsForeignLayerType(type));
  DCHECK(GetLayer());
  DCHECK(!IsCacheable());
  // TODO(959734): This CHECK is intended to find stack traces that are causing
  // a segfault.
  CHECK(GetLayer());
}

ForeignLayerDisplayItem::~ForeignLayerDisplayItem() {
  delete &Client();
}

cc::Layer* ForeignLayerDisplayItem::GetLayer() const {
  return static_cast<const ForeignLayerDisplayItemClient&>(Client()).GetLayer();
}

bool ForeignLayerDisplayItem::Equals(const DisplayItem& other) const {
  return GetType() == other.GetType() &&
         GetLayer() ==
             static_cast<const ForeignLayerDisplayItem&>(other).GetLayer();
}

#if DCHECK_IS_ON()
void ForeignLayerDisplayItem::PropertiesAsJSON(JSONObject& json) const {
  DisplayItem::PropertiesAsJSON(json);
  json.SetInteger("layer", GetLayer()->id());
  json.SetDouble("offset_x", Offset().X());
  json.SetDouble("offset_y", Offset().Y());
}
#endif

void RecordForeignLayer(GraphicsContext& context,
                        DisplayItem::Type type,
                        scoped_refptr<cc::Layer> layer,
                        const FloatPoint& offset,
                        const base::Optional<PropertyTreeState>& properties) {
  PaintController& paint_controller = context.GetPaintController();
  if (paint_controller.DisplayItemConstructionIsDisabled())
    return;

  // This is like ScopedPaintChunkProperties but uses null id because foreign
  // layer chunk doesn't need an id nor a client.
  base::Optional<PropertyTreeState> previous_properties;
  if (properties) {
    previous_properties.emplace(paint_controller.CurrentPaintChunkProperties());
    paint_controller.UpdateCurrentPaintChunkProperties(base::nullopt,
                                                       *properties);
  }
  paint_controller.CreateAndAppend<ForeignLayerDisplayItem>(
      type, std::move(layer), offset);
  if (properties) {
    paint_controller.UpdateCurrentPaintChunkProperties(base::nullopt,
                                                       *previous_properties);
  }
}

}  // namespace blink
