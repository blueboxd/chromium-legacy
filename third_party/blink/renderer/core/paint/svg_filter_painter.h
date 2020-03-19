// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_FILTER_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_FILTER_PAINTER_H_

#include <memory>
#include "base/macros.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class FilterData;
class GraphicsContext;
class LayoutObject;
class LayoutSVGResourceFilter;
class PaintController;

class SVGFilterRecordingContext {
  USING_FAST_MALLOC(SVGFilterRecordingContext);

 public:
  explicit SVGFilterRecordingContext(const PaintInfo&);
  ~SVGFilterRecordingContext();

  const PaintInfo& GetPaintInfo() const { return paint_info_; }
  sk_sp<PaintRecord> GetPaintRecord(const PaintInfo&);

 private:
  std::unique_ptr<PaintController> paint_controller_;
  std::unique_ptr<GraphicsContext> context_;
  PaintInfo paint_info_;
  DISALLOW_COPY_AND_ASSIGN(SVGFilterRecordingContext);
};

class SVGFilterPainter {
  STACK_ALLOCATED();

 public:
  SVGFilterPainter(LayoutSVGResourceFilter& filter) : filter_(filter) {}

  // Returns the FilterData for the filter effect, or null if the
  // filter is invalid.
  FilterData* PrepareEffect(const LayoutObject&);

 private:
  LayoutSVGResourceFilter& filter_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_SVG_FILTER_PAINTER_H_
