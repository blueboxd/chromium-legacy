// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/layout_ng_flexible_box.h"

#include "third_party/blink/renderer/core/layout/layout_analyzer.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/ng_out_of_flow_positioned_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

LayoutNGFlexibleBox::LayoutNGFlexibleBox(Element* element)
    : LayoutNGMixin<LayoutBlock>(element) {}

bool LayoutNGFlexibleBox::HasTopOverflow() const {
  if (IsHorizontalWritingMode())
    return StyleRef().ResolvedIsColumnReverseFlexDirection();
  return StyleRef().IsLeftToRightDirection() ==
         StyleRef().ResolvedIsRowReverseFlexDirection();
}

bool LayoutNGFlexibleBox::HasLeftOverflow() const {
  if (IsHorizontalWritingMode()) {
    return StyleRef().IsLeftToRightDirection() ==
           StyleRef().ResolvedIsRowReverseFlexDirection();
  }
  return StyleRef().ResolvedIsColumnReverseFlexDirection();
}

void LayoutNGFlexibleBox::UpdateBlockLayout(bool relayout_children) {
  LayoutAnalyzer::BlockScope analyzer(*this);

  if (IsOutOfFlowPositioned()) {
    UpdateOutOfFlowBlockLayout();
    return;
  }

  UpdateInFlowBlockLayout();
}

namespace {

void MergeAnonymousFlexItems(LayoutObject* remove_child) {
  // When we remove a flex item, and the previous and next siblings of the item
  // are text nodes wrapped in anonymous flex items, the adjacent text nodes
  // need to be merged into the same flex item.
  LayoutObject* prev = remove_child->PreviousSibling();
  if (!prev || !prev->IsAnonymousBlock())
    return;
  LayoutObject* next = remove_child->NextSibling();
  if (!next || !next->IsAnonymousBlock())
    return;
  ToLayoutBoxModelObject(next)->MoveAllChildrenTo(ToLayoutBoxModelObject(prev));
  To<LayoutBlockFlow>(next)->DeleteLineBoxTree();
  next->Destroy();
}

}  // namespace

void LayoutNGFlexibleBox::RemoveChild(LayoutObject* child) {
  if (!DocumentBeingDestroyed() &&
      !StyleRef().IsDeprecatedFlexboxUsingFlexLayout())
    MergeAnonymousFlexItems(child);

  LayoutBlock::RemoveChild(child);
}

}  // namespace blink
