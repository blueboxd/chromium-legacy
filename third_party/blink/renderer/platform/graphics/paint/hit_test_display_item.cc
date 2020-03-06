// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/hit_test_display_item.h"

#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"

namespace blink {

void HitTestDisplayItem::Record(GraphicsContext& context,
                                const DisplayItemClient& client,
                                const HitTestRect& hit_test_rect) {
  auto& paint_controller = context.GetPaintController();
  if (!paint_controller.UseCachedItemIfPossible(client, DisplayItem::kHitTest))
    paint_controller.CreateAndAppend<HitTestDisplayItem>(client, hit_test_rect);
}

#if DCHECK_IS_ON()
void HitTestDisplayItem::PropertiesAsJSON(JSONObject& json) const {
  DisplayItem::PropertiesAsJSON(json);
  json.SetString("hitTestRect", hit_test_rect_.ToString());
}
#endif

bool HitTestDisplayItem::Equals(const DisplayItem& other) const {
  return DisplayItem::Equals(other) &&
         hit_test_rect_ ==
             static_cast<const HitTestDisplayItem&>(other).hit_test_rect_;
}

}  // namespace blink
