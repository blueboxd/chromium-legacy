/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights
 * reserved.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@fas.harvard.edu>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Randall Jesup <rjesup@wgate.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Josh Soref <timeless@mac.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#include "third_party/blink/renderer/core/paint/paint_layer_stacking_node.h"

#include <algorithm>
#include <memory>

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"

namespace blink {

// FIXME: This should not require PaintLayer. There is currently a cycle where
// in order to determine if we isStacked() we have to ask the paint
// layer about some of its state.
PaintLayerStackingNode::PaintLayerStackingNode(PaintLayer& layer)
    : layer_(layer), z_order_lists_dirty_(true) {
  DCHECK(layer.GetLayoutObject().StyleRef().IsStackingContext());
}

PaintLayerStackingNode::~PaintLayerStackingNode() {
#if DCHECK_IS_ON()
  if (!layer_.GetLayoutObject().DocumentBeingDestroyed())
    UpdateStackingParentForZOrderLists(nullptr);
#endif
}

PaintLayerCompositor* PaintLayerStackingNode::Compositor() const {
  DCHECK(layer_.GetLayoutObject().View());
  if (!layer_.GetLayoutObject().View())
    return nullptr;
  return layer_.GetLayoutObject().View()->Compositor();
}

void PaintLayerStackingNode::DirtyZOrderLists() {
#if DCHECK_IS_ON()
  DCHECK(layer_.LayerListMutationAllowed());
  UpdateStackingParentForZOrderLists(nullptr);
#endif

  pos_z_order_list_.clear();
  neg_z_order_list_.clear();
  z_order_lists_dirty_ = true;

  if (!layer_.GetLayoutObject().DocumentBeingDestroyed() && Compositor())
    Compositor()->SetNeedsCompositingUpdate(kCompositingUpdateRebuildTree);
}

void PaintLayerStackingNode::DirtyStackingContextZOrderLists(
    PaintLayer& layer) {
  auto* stacking_context = layer.AncestorStackingContext();
  if (!stacking_context)
    return;

  // This invalidation code intentionally refers to stale state.
  DisableCompositingQueryAsserts disabler;

  // Changes of stacking may result in graphics layers changing size
  // due to new contents painting into them.
  if (auto* mapping = stacking_context->GetCompositedLayerMapping())
    mapping->SetNeedsGraphicsLayerUpdate(kGraphicsLayerUpdateSubtree);

  if (stacking_context->StackingNode())
    stacking_context->StackingNode()->DirtyZOrderLists();
}

void PaintLayerStackingNode::RebuildZOrderLists() {
#if DCHECK_IS_ON()
  DCHECK(layer_.LayerListMutationAllowed());
#endif
  DCHECK(z_order_lists_dirty_);

  for (PaintLayer* child = layer_.FirstChild(); child;
       child = child->NextSibling())
    CollectLayers(*child);

  auto CompareZIndex = [](PaintLayer* first, PaintLayer* second) {
    return first->GetLayoutObject().StyleRef().ZIndex() <
           second->GetLayoutObject().StyleRef().ZIndex();
  };

  // Sort the two lists.
  std::stable_sort(pos_z_order_list_.begin(), pos_z_order_list_.end(),
                   CompareZIndex);
  std::stable_sort(neg_z_order_list_.begin(), neg_z_order_list_.end(),
                   CompareZIndex);

  // Append layers for top layer elements after normal layer collection, to
  // ensure they are on top regardless of z-indexes.  The layoutObjects of top
  // layer elements are children of the view, sorted in top layer stacking
  // order.
  if (layer_.IsRootLayer()) {
    LayoutBlockFlow* root_block = layer_.GetLayoutObject().View();
    // If the viewport is paginated, everything (including "top-layer" elements)
    // gets redirected to the flow thread. So that's where we have to look, in
    // that case.
    if (LayoutBlockFlow* multi_column_flow_thread =
            root_block->MultiColumnFlowThread())
      root_block = multi_column_flow_thread;
    for (LayoutObject* child = root_block->FirstChild(); child;
         child = child->NextSibling()) {
      Element* child_element =
          (child->GetNode() && child->GetNode()->IsElementNode())
              ? ToElement(child->GetNode())
              : nullptr;
      if (child_element && child_element->IsInTopLayer() &&
          child->StyleRef().IsStacked()) {
        pos_z_order_list_.push_back(ToLayoutBoxModelObject(child)->Layer());
      }
    }
  }

#if DCHECK_IS_ON()
  UpdateStackingParentForZOrderLists(this);
#endif

  z_order_lists_dirty_ = false;
}

void PaintLayerStackingNode::CollectLayers(PaintLayer& paint_layer) {
  if (paint_layer.IsInTopLayer())
    return;

  const ComputedStyle& style = paint_layer.GetLayoutObject().StyleRef();

  if (style.IsStacked()) {
    auto& list = style.ZIndex() >= 0 ? pos_z_order_list_ : neg_z_order_list_;
    list.push_back(&paint_layer);
  }

  if (style.IsStackingContext())
    return;

  for (PaintLayer* child = paint_layer.FirstChild(); child;
       child = child->NextSibling())
    CollectLayers(*child);
}

#if DCHECK_IS_ON()
void PaintLayerStackingNode::UpdateStackingParentForZOrderLists(
    PaintLayerStackingNode* stacking_parent) {
  for (auto* layer : pos_z_order_list_)
    layer->SetStackingParent(stacking_parent);
  for (auto* layer : neg_z_order_list_)
    layer->SetStackingParent(stacking_parent);
}

#endif

bool PaintLayerStackingNode::StyleDidChange(PaintLayer& paint_layer,
                                            const ComputedStyle* old_style) {
  bool was_stacking_context = false;
  bool was_stacked = false;
  int old_z_index = 0;
  if (old_style) {
    was_stacking_context = old_style->IsStackingContext();
    old_z_index = old_style->ZIndex();
    was_stacked = old_style->IsStacked();
  }

  const ComputedStyle& new_style = paint_layer.GetLayoutObject().StyleRef();

  bool should_be_stacking_context = new_style.IsStackingContext();
  bool should_be_stacked = new_style.IsStacked();
  if (should_be_stacking_context == was_stacking_context &&
      was_stacked == should_be_stacked && old_z_index == new_style.ZIndex())
    return false;

  // Need to force requirements update, due to change of stacking order.
  paint_layer.SetNeedsCompositingRequirementsUpdate();
  DirtyStackingContextZOrderLists(paint_layer);

  if (paint_layer.StackingNode())
    paint_layer.StackingNode()->DirtyZOrderLists();

  if (was_stacked != should_be_stacked) {
    if (!paint_layer.GetLayoutObject().DocumentBeingDestroyed() &&
        !paint_layer.IsRootLayer() && paint_layer.Compositor()) {
      paint_layer.Compositor()->SetNeedsCompositingUpdate(
          kCompositingUpdateRebuildTree);
    }
  }
  return true;
}

void PaintLayerStackingNode::UpdateZOrderLists() {
  if (z_order_lists_dirty_)
    RebuildZOrderLists();
}

}  // namespace blink
