// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/intersection_observer/intersection_geometry.h"

#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"

namespace blink {

namespace {

bool IsContainingBlockChainDescendant(LayoutObject* descendant,
                                      LayoutObject* ancestor) {
  LocalFrame* ancestor_frame = ancestor->GetDocument().GetFrame();
  LocalFrame* descendant_frame = descendant->GetDocument().GetFrame();
  if (ancestor_frame != descendant_frame)
    return false;

  while (descendant && descendant != ancestor)
    descendant = descendant->ContainingBlock();
  return descendant;
}

void MapRectUpToDocument(PhysicalRect& rect, const LayoutObject& descendant) {
  rect = descendant.LocalToAncestorRect(rect, nullptr);
}

void MapRectDownToDocument(PhysicalRect& rect, const Document& document) {
  rect = document.GetLayoutView()->AbsoluteToLocalRect(
      rect, kTraverseDocumentBoundaries);
}

LayoutUnit ComputeMargin(const Length& length, LayoutUnit reference_length) {
  if (length.IsPercent()) {
    return LayoutUnit(static_cast<int>(reference_length.ToFloat() *
                                       length.Percent() / 100.0));
  }
  DCHECK(length.IsFixed());
  return LayoutUnit(length.IntValue());
}

LayoutView* LocalRootView(Element& element) {
  LocalFrame* frame = element.GetDocument().GetFrame();
  LocalFrame* frame_root = frame ? &frame->LocalFrameRoot() : nullptr;
  return frame_root ? frame_root->ContentLayoutObject() : nullptr;
}

bool ComputeIsVisible(LayoutObject* target, const PhysicalRect& rect) {
  DCHECK(RuntimeEnabledFeatures::IntersectionObserverV2Enabled());
  if (target->GetDocument().GetFrame()->LocalFrameRoot().GetOcclusionState() !=
      FrameOcclusionState::kGuaranteedNotOccluded) {
    return false;
  }
  if (target->HasDistortingVisualEffects())
    return false;
  // TODO(layout-dev): This should hit-test the intersection rect, not the
  // target rect; it's not helpful to know that the portion of the target that
  // is clipped is also occluded.
  HitTestResult result(target->HitTestForOcclusion(rect));
  Node* hit_node = result.InnerNode();
  if (!hit_node || hit_node == target->GetNode())
    return true;
  // TODO(layout-dev): This IsDescendantOf tree walk could be optimized by
  // stopping when hit_node's containing LayoutBlockFlow is reached.
  if (target->IsLayoutInline())
    return hit_node->IsDescendantOf(target->GetNode());
  return false;
}

static const unsigned kConstructorFlagsMask =
    IntersectionGeometry::kShouldReportRootBounds |
    IntersectionGeometry::kShouldComputeVisibility |
    IntersectionGeometry::kShouldTrackFractionOfRoot |
    IntersectionGeometry::kShouldUseReplacedContentRect |
    IntersectionGeometry::kShouldConvertToCSSPixels;

}  // namespace

IntersectionGeometry::IntersectionGeometry(Element* root_element,
                                           Element& target_element,
                                           const Vector<Length>& root_margin,
                                           const Vector<float>& thresholds,
                                           unsigned flags)
    : flags_(flags & kConstructorFlagsMask),
      intersection_ratio_(0),
      threshold_index_(0) {
  DCHECK(root_margin.IsEmpty() || root_margin.size() == 4);
  ComputeGeometry(root_element, target_element, root_margin, thresholds);
}

IntersectionGeometry::~IntersectionGeometry() = default;

void IntersectionGeometry::ComputeGeometry(Element* root_element,
                                           Element& target_element,
                                           const Vector<Length>& root_margin,
                                           const Vector<float>& thresholds) {
  LayoutObject* target = target_element.GetLayoutObject();
  LayoutObject* root;
  if (root_element) {
    root = root_element->GetLayoutObject();
    flags_ &= ~kRootIsImplicit;
  } else {
    root = LocalRootView(target_element);
    flags_ |= kRootIsImplicit;
  }
  if (!target_element.isConnected())
    return;
  if (root_element && !root_element->isConnected())
    return;
  if (!root || !root->IsBox())
    return;
  if (!target || (!target->IsBoxModelObject() && !target->IsText()))
    return;
  if (root_element && !IsContainingBlockChainDescendant(target, root))
    return;
  // If the target is inside a locked subtree, it isn't ever visible.
  if (UNLIKELY(DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(
          target_element))) {
    return;
  }

  DCHECK(!target_element.GetDocument().View()->NeedsLayout());

  target_rect_ = InitializeTargetRect(target);
  intersection_rect_ = target_rect_;
  root_rect_ = InitializeRootRect(root, root_margin);
  bool does_intersect =
      ClipToRoot(root, target, root_rect_, intersection_rect_);
  MapRectUpToDocument(target_rect_, *target);
  if (does_intersect) {
    if (RootIsImplicit())
      MapRectDownToDocument(intersection_rect_, target->GetDocument());
    else
      MapRectUpToDocument(intersection_rect_, *root);
  } else {
    intersection_rect_ = PhysicalRect();
  }
  MapRectUpToDocument(root_rect_, *root);

  // Some corner cases for threshold index:
  //   - If target rect is zero area, because it has zero width and/or zero
  //     height,
  //     only two states are recognized:
  //     - 0 means not intersecting.
  //     - 1 means intersecting.
  //     No other threshold crossings are possible.
  //   - Otherwise:
  //     - If root and target do not intersect, the threshold index is 0.

  //     - If root and target intersect but the intersection has zero-area
  //       (i.e., they have a coincident edge or corner), we consider the
  //       intersection to have "crossed" a zero threshold, but not crossed
  //       any non-zero threshold.

  if (does_intersect) {
    const PhysicalRect& comparison_rect =
        ShouldTrackFractionOfRoot() ? root_rect_ : target_rect_;
    if (comparison_rect.IsEmpty()) {
      intersection_ratio_ = 1;
    } else {
      const PhysicalSize& intersection_size = intersection_rect_.size;
      const float intersection_area = intersection_size.width.ToFloat() *
                                      intersection_size.height.ToFloat();
      const PhysicalSize& comparison_size = comparison_rect.size;
      const float area_of_interest =
          comparison_size.width.ToFloat() * comparison_size.height.ToFloat();
      intersection_ratio_ = intersection_area / area_of_interest;
    }
    threshold_index_ =
        FirstThresholdGreaterThan(intersection_ratio_, thresholds);
  } else {
    intersection_ratio_ = 0;
    threshold_index_ = 0;
  }
  if (IsIntersecting() && ShouldComputeVisibility() &&
      ComputeIsVisible(target, target_rect_))
    flags_ |= kIsVisible;

  if (flags_ & kShouldConvertToCSSPixels) {
    FloatRect target_float_rect(target_rect_);
    AdjustForAbsoluteZoom::AdjustFloatRect(target_float_rect, *target);
    target_rect_ = PhysicalRect::EnclosingRect(target_float_rect);
    FloatRect intersection_float_rect(intersection_rect_);
    AdjustForAbsoluteZoom::AdjustFloatRect(intersection_float_rect, *target);
    intersection_rect_ = PhysicalRect::EnclosingRect(intersection_float_rect);
    FloatRect root_float_rect(root_rect_);
    AdjustForAbsoluteZoom::AdjustFloatRect(root_float_rect, *root);
    root_rect_ = PhysicalRect::EnclosingRect(root_float_rect);
  }
}

PhysicalRect IntersectionGeometry::InitializeTargetRect(LayoutObject* target) {
  if ((flags_ & kShouldUseReplacedContentRect) &&
      target->IsLayoutEmbeddedContent()) {
    return ToLayoutEmbeddedContent(target)->ReplacedContentRect();
  }
  if (target->IsBox())
    return PhysicalRect(ToLayoutBoxModelObject(target)->BorderBoundingBox());
  if (target->IsLayoutInline()) {
    return target->AbsoluteToLocalRect(
        PhysicalRect::EnclosingRect(target->AbsoluteBoundingBoxFloatRect()));
  }
  return ToLayoutText(target)->PhysicalLinesBoundingBox();
}

PhysicalRect IntersectionGeometry::InitializeRootRect(
    LayoutObject* root,
    const Vector<Length>& margin) {
  PhysicalRect result;
  if (root->IsLayoutView() && root->GetDocument().IsInMainFrame()) {
    // The main frame is a bit special as the scrolling viewport can differ in
    // size from the LayoutView itself. There's two situations this occurs in:
    // 1) The ForceZeroLayoutHeight quirk setting is used in Android WebView for
    // compatibility and sets the initial-containing-block's (a.k.a.
    // LayoutView) height to 0. Thus, we can't use its size for intersection
    // testing. Use the FrameView geometry instead.
    // 2) An element wider than the ICB can cause us to resize the FrameView so
    // we can zoom out to fit the entire element width.
    result = ToLayoutView(root)->OverflowClipRect(PhysicalOffset());
  } else if (root->IsBox() && root->HasOverflowClip()) {
    result = ToLayoutBox(root)->PhysicalContentBoxRect();
  } else {
    result = PhysicalRect(ToLayoutBoxModelObject(root)->BorderBoundingBox());
  }
  ApplyRootMargin(result, margin);
  return result;
}

void IntersectionGeometry::ApplyRootMargin(PhysicalRect& rect,
                                           const Vector<Length>& margin) {
  if (margin.IsEmpty())
    return;

  // TODO(szager): Make sure the spec is clear that left/right margins are
  // resolved against width and not height.
  LayoutRectOutsets outsets(ComputeMargin(margin[0], rect.Height()),
                            ComputeMargin(margin[1], rect.Width()),
                            ComputeMargin(margin[2], rect.Height()),
                            ComputeMargin(margin[3], rect.Width()));
  rect.Expand(outsets);
}

bool IntersectionGeometry::ClipToRoot(LayoutObject* root,
                                      LayoutObject* target,
                                      const PhysicalRect& root_rect,
                                      PhysicalRect& intersection_rect) {
  // Map and clip rect into root element coordinates.
  // TODO(szager): the writing mode flipping needs a test.
  LayoutBox* local_ancestor = nullptr;
  if (!RootIsImplicit() || root->GetDocument().IsInMainFrame())
    local_ancestor = ToLayoutBox(root);

  LayoutView* layout_view = target->GetDocument().GetLayoutView();

  unsigned flags = kDefaultVisualRectFlags | kEdgeInclusive;
  if (!layout_view->NeedsPaintPropertyUpdate() &&
      !layout_view->DescendantNeedsPaintPropertyUpdate()) {
    flags |= kUseGeometryMapper;
  }
  bool does_intersect = target->MapToVisualRectInAncestorSpace(
      local_ancestor, intersection_rect, static_cast<VisualRectFlags>(flags));
  if (!does_intersect || !local_ancestor)
    return does_intersect;
  if (local_ancestor->HasOverflowClip()) {
    intersection_rect.Move(
        -PhysicalOffset(LayoutPoint(local_ancestor->ScrollOrigin()) +
                        local_ancestor->ScrolledContentOffset()));
  }
  LayoutRect root_clip_rect = root_rect.ToLayoutRect();
  // TODO(szager): This flipping seems incorrect because root_rect is already
  // physical.
  local_ancestor->DeprecatedFlipForWritingMode(root_clip_rect);
  return does_intersect &
         intersection_rect.InclusiveIntersect(PhysicalRect(root_clip_rect));
}

unsigned IntersectionGeometry::FirstThresholdGreaterThan(
    float ratio,
    const Vector<float>& thresholds) const {
  unsigned result = 0;
  while (result < thresholds.size() && thresholds[result] <= ratio)
    ++result;
  return result;
}

}  // namespace blink
