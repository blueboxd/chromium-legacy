// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

#if DCHECK_IS_ON()
static bool g_list_modification_check_disabled = false;
DisableListModificationCheck::DisableListModificationCheck()
    : disabler_(&g_list_modification_check_disabled, true) {}
#endif

DrawingRecorder::DrawingRecorder(GraphicsContext& context,
                                 const DisplayItemClient& display_item_client,
                                 DisplayItem::Type display_item_type)
    : context_(context),
      client_(display_item_client),
      type_(display_item_type)
#if DCHECK_IS_ON()
      ,
      initial_display_item_list_size_(
          context_.GetPaintController().NewDisplayItemList().size())
#endif
{
  // Must check DrawingRecorder::UseCachedDrawingIfPossible before creating the
  // DrawingRecorder.
  DCHECK(RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() ||
         context_.GetPaintController().ShouldForcePaintForBenchmark() ||
         !UseCachedDrawingIfPossible(context_, client_, type_));

  DCHECK(DisplayItem::IsDrawingType(display_item_type));

  context.SetInDrawingRecorder(true);
  context.BeginRecording(FloatRect());

  if (context.Printing()) {
    DOMNodeId dom_node_id = display_item_client.OwnerNodeId();
    if (dom_node_id != kInvalidDOMNodeId) {
      dom_node_id_to_restore_ = context.GetDOMNodeId();
      context.SetDOMNodeId(dom_node_id);
    }
  }
}

DrawingRecorder::~DrawingRecorder() {
  if (context_.Printing() && dom_node_id_to_restore_)
    context_.SetDOMNodeId(dom_node_id_to_restore_.value());

  context_.SetInDrawingRecorder(false);

#if DCHECK_IS_ON()
  if (!g_list_modification_check_disabled) {
    DCHECK(initial_display_item_list_size_ ==
           context_.GetPaintController().NewDisplayItemList().size());
  }
#endif

  context_.GetPaintController().CreateAndAppend<DrawingDisplayItem>(
      client_, type_, context_.EndRecording());
}

}  // namespace blink
