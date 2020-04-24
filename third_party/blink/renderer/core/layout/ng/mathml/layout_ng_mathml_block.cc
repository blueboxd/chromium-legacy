// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/mathml/layout_ng_mathml_block.h"

#include "third_party/blink/renderer/core/layout/layout_analyzer.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/mathml/mathml_element.h"

namespace blink {

LayoutNGMathMLBlock::LayoutNGMathMLBlock(Element* element)
    : LayoutNGMixin<LayoutBlock>(element) {
  DCHECK(element);
}

void LayoutNGMathMLBlock::UpdateBlockLayout(bool relayout_children) {
  LayoutAnalyzer::BlockScope analyzer(*this);

  if (IsOutOfFlowPositioned()) {
    UpdateOutOfFlowBlockLayout();
    return;
  }

  UpdateInFlowBlockLayout();
}

bool LayoutNGMathMLBlock::IsOfType(LayoutObjectType type) const {
  return type == kLayoutObjectMathML ||
         (type == kLayoutObjectMathMLRoot && GetNode() &&
          GetNode()->HasTagName(mathml_names::kMathTag)) ||
         LayoutNGMixin<LayoutBlock>::IsOfType(type);
}

bool LayoutNGMathMLBlock::IsChildAllowed(LayoutObject* child,
                                         const ComputedStyle&) const {
  return child->GetNode() && IsA<MathMLElement>(child->GetNode());
}

bool LayoutNGMathMLBlock::CanHaveChildren() const {
  if (GetNode() && GetNode()->HasTagName(mathml_names::kMspaceTag))
    return false;
  return LayoutNGMixin<LayoutBlock>::CanHaveChildren();
}

}  // namespace blink
