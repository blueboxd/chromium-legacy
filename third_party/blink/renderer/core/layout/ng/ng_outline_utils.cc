// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/ng_outline_utils.h"

#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

bool NGOutlineUtils::HasPaintedOutline(const ComputedStyle& style,
                                       const Node* node) {
  if (!style.HasOutline() || style.Visibility() != EVisibility::kVisible)
    return false;
  if (style.OutlineStyleIsAuto() &&
      !LayoutTheme::GetTheme().ShouldDrawDefaultFocusRing(node, style))
    return false;
  return true;
}

bool NGOutlineUtils::ShouldPaintOutline(
    const NGPhysicalBoxFragment& physical_fragment) {
  const LayoutObject* layout_object = physical_fragment.GetLayoutObject();
  DCHECK(layout_object);
  if (!layout_object->IsLayoutInline())
    return true;

  // A |LayoutInline| can be split across multiple objects. The first fragment
  // produced should paint the outline for *all* fragments.
  if (layout_object->IsElementContinuation()) {
    // If the |LayoutInline|'s continuation-root generated a fragment, we
    // shouldn't paint the outline.
    if (layout_object->ContinuationRoot()->FirstInlineFragment())
      return false;
  }

  DCHECK(layout_object->FirstInlineFragment());
  return &layout_object->FirstInlineFragment()->PhysicalFragment() ==
         &physical_fragment;
}

}  // namespace blink
