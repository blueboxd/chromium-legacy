// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_recalc_root.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"

namespace blink {

Element& StyleRecalcRoot::RootElement() const {
  Node* root_node = GetRootNode();
  DCHECK(root_node);
  if (root_node->IsDocumentNode())
    return *root_node->GetDocument().documentElement();
  if (root_node->IsPseudoElement()) {
    // We could possibly have called UpdatePseudoElement, but start at the
    // originating element for simplicity.
    return *root_node->parentElement();
  }
  if (root_node->IsInShadowTree()) {
    // Since we traverse in light tree order, we might need to traverse slotted
    // shadow host children for inheritance for which the recalc root is not an
    // ancestor. Since we might re-slot slots, we need to start at the outermost
    // shadow host.
    TreeScope* tree_scope = &root_node->GetTreeScope();
    while (!tree_scope->ParentTreeScope()->RootNode().IsDocumentNode())
      tree_scope = tree_scope->ParentTreeScope();
    return To<ShadowRoot>(tree_scope->RootNode()).host();
  }
  if (root_node->IsTextNode())
    return *root_node->parentElement();
  return To<Element>(*root_node);
}

#if DCHECK_IS_ON()
ContainerNode* StyleRecalcRoot::Parent(const Node& node) const {
  return node.ParentOrShadowHostNode();
}

bool StyleRecalcRoot::IsChildDirty(const ContainerNode& node) const {
  return node.ChildNeedsStyleRecalc();
}
#endif  // DCHECK_IS_ON()

bool StyleRecalcRoot::IsDirty(const Node& node) const {
  return node.NeedsStyleRecalc();
}

void StyleRecalcRoot::ClearChildDirtyForAncestors(ContainerNode& parent) const {
  for (ContainerNode* ancestor = &parent; ancestor;
       ancestor = ancestor->ParentOrShadowHostNode()) {
    ancestor->ClearChildNeedsStyleRecalc();
    DCHECK(!ancestor->NeedsStyleRecalc());
  }
}

}  // namespace blink
