// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_MASK_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_MASK_PAINTER_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class DisplayItemClient;
class GraphicsContext;
class LayoutObject;

class SVGMaskPainter {
  STACK_ALLOCATED();

 public:
  SVGMaskPainter(GraphicsContext&,
                 const LayoutObject&,
                 const DisplayItemClient&);
  ~SVGMaskPainter();

 private:
  GraphicsContext& context_;
  const LayoutObject& layout_object_;
  const DisplayItemClient& display_item_client_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_MASK_PAINTER_H_
