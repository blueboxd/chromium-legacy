// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_TEXT_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_TEXT_PAINTER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/text_painter_base.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"

namespace blink {

class NGPhysicalTextFragment;
struct NGTextFragmentPaintInfo;

// Text painter for LayoutNG, logic shared between legacy layout and LayoutNG
// is implemented in the TextPainterBase base class.
// Operates on NGPhysicalTextFragments and only paints text and decorations.
// Border painting etc is handled by the NGTextFragmentPainter class.
// TODO(layout-dev): Does this distinction make sense?
class CORE_EXPORT NGTextPainter : public TextPainterBase {
  STACK_ALLOCATED();

 public:
  NGTextPainter(GraphicsContext& context,
                const Font& font,
                const NGPaintFragment& paint_fragment,
                const PhysicalOffset& text_origin,
                const PhysicalRect& text_bounds,
                bool horizontal)
      : TextPainterBase(context, font, text_origin, text_bounds, horizontal),
        paint_fragment_(paint_fragment),
        fragment_(
            To<NGPhysicalTextFragment>(paint_fragment.PhysicalFragment())) {}
  ~NGTextPainter() = default;

  void ClipDecorationsStripe(float upper,
                             float stripe_width,
                             float dilation) override;
  void Paint(unsigned start_offset,
             unsigned end_offset,
             unsigned length,
             const TextPaintStyle&,
             DOMNodeId);

  static TextPaintStyle TextPaintingStyle(const NGPhysicalTextFragment*,
                                          const ComputedStyle&,
                                          const PaintInfo&);
  static TextPaintStyle SelectionPaintingStyle(
      const NGPhysicalTextFragment*,
      bool have_selection,
      const PaintInfo&,
      const TextPaintStyle& text_style);

 private:
  template <PaintInternalStep step>
  void PaintInternalFragment(NGTextFragmentPaintInfo&,
                             unsigned from,
                             unsigned to,
                             DOMNodeId node_id);

  template <PaintInternalStep step>
  void PaintInternal(unsigned start_offset,
                     unsigned end_offset,
                     unsigned truncation_point,
                     DOMNodeId node_id);

  void PaintEmphasisMarkForCombinedText();

  const NGPaintFragment& paint_fragment_;
  const NGPhysicalTextFragment& fragment_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_TEXT_PAINTER_H_
