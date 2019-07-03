// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/frame_view.h"

#include "third_party/blink/renderer/core/frame/frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_geometry.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"

namespace blink {

Frame& FrameView::GetFrame() const {
  if (const LocalFrameView* lfv = DynamicTo<LocalFrameView>(this))
    return lfv->GetFrame();
  return DynamicTo<RemoteFrameView>(this)->GetFrame();
}

bool FrameView::CanThrottleRenderingForPropagation() const {
  if (CanThrottleRendering())
    return true;
  if (!RuntimeEnabledFeatures::RenderingPipelineThrottlingEnabled())
    return false;
  LocalFrame* parent_frame = DynamicTo<LocalFrame>(GetFrame().Tree().Parent());
  if (!parent_frame)
    return false;
  Frame& frame = GetFrame();
  LayoutEmbeddedContent* owner = frame.OwnerLayoutObject();
  return !owner && frame.IsCrossOriginSubframe();
}

void FrameView::UpdateViewportIntersection(unsigned flags,
                                           bool needs_occlusion_tracking) {
  if (!(flags & IntersectionObservation::kImplicitRootObserversNeedUpdate))
    return;
  // This should only run in child frames.
  Frame& frame = GetFrame();
  HTMLFrameOwnerElement* owner_element = frame.DeprecatedLocalOwner();
  if (!owner_element)
    return;
  Document& owner_document = owner_element->GetDocument();
  IntRect viewport_intersection;
  DocumentLifecycle::LifecycleState parent_lifecycle_state =
      owner_document.Lifecycle().GetState();
  FrameOcclusionState occlusion_state =
      owner_document.GetFrame()->GetOcclusionState();
  bool should_compute_occlusion =
      needs_occlusion_tracking &&
      occlusion_state == FrameOcclusionState::kGuaranteedNotOccluded &&
      parent_lifecycle_state >= DocumentLifecycle::kPrePaintClean &&
      RuntimeEnabledFeatures::IntersectionObserverV2Enabled();

  LayoutEmbeddedContent* owner_layout_object =
      owner_element->GetLayoutEmbeddedContent();
  if (!owner_layout_object || owner_layout_object->ContentSize().IsEmpty()) {
    // The frame is detached from layout, not visible, or zero size; leave
    // viewport_intersection empty, and signal the frame as occluded if
    // necessary.
    occlusion_state = FrameOcclusionState::kPossiblyOccluded;
  } else if (parent_lifecycle_state >= DocumentLifecycle::kLayoutClean) {
    unsigned geometry_flags =
        IntersectionGeometry::kShouldUseReplacedContentRect;
    if (should_compute_occlusion)
      geometry_flags |= IntersectionGeometry::kShouldComputeVisibility;

    IntersectionGeometry geometry(nullptr, *owner_element, {},
                                  {IntersectionObserver::kMinimumThreshold},
                                  geometry_flags);
    // geometry.IntersectionRect() is in absolute coordinates of the owning
    // document. Map it down to absolute coordinates in the child document.
    PhysicalRect intersection_rect = owner_layout_object->AncestorToLocalRect(
        nullptr, geometry.IntersectionRect());
    // Map from the box coordinates of the owner to the inner frame.
    intersection_rect.Move(-owner_layout_object->PhysicalContentBoxOffset());
    // Don't let EnclosingIntRect turn an empty rect into a non-empty one.
    if (intersection_rect.IsEmpty()) {
      viewport_intersection =
          IntRect(FlooredIntPoint(intersection_rect.offset), IntSize());
    } else {
      viewport_intersection = EnclosingIntRect(intersection_rect);
    }
    if (should_compute_occlusion && !geometry.IsVisible())
      occlusion_state = FrameOcclusionState::kPossiblyOccluded;
  } else if (occlusion_state == FrameOcclusionState::kGuaranteedNotOccluded) {
    // If the parent LocalFrameView is throttled and out-of-date, then we can't
    // get any useful information.
    occlusion_state = FrameOcclusionState::kUnknown;
  }

  SetViewportIntersection(viewport_intersection, occlusion_state);

  UpdateFrameVisibility(!viewport_intersection.IsEmpty());

  // We don't throttle 0x0 or display:none iframes, because in practice they are
  // sometimes used to drive UI logic.
  bool hidden_for_throttling = viewport_intersection.IsEmpty() &&
                               !FrameRect().IsEmpty() && owner_layout_object;
  bool subtree_throttled = false;
  Frame* parent_frame = GetFrame().Tree().Parent();
  if (parent_frame && parent_frame->View()) {
    subtree_throttled =
        parent_frame->View()->CanThrottleRenderingForPropagation();
  }
  UpdateRenderThrottlingStatus(hidden_for_throttling, subtree_throttled);
}

void FrameView::UpdateFrameVisibility(bool intersects_viewport) {
  blink::mojom::FrameVisibility frame_visibility;
  if (LifecycleUpdatesThrottled())
    return;
  if (IsVisible()) {
    frame_visibility =
        intersects_viewport
            ? blink::mojom::FrameVisibility::kRenderedInViewport
            : blink::mojom::FrameVisibility::kRenderedOutOfViewport;
  } else {
    frame_visibility = blink::mojom::FrameVisibility::kNotRendered;
  }
  if (frame_visibility != frame_visibility_) {
    frame_visibility_ = frame_visibility;
    if (FrameClient* client = GetFrame().Client())
      client->VisibilityChanged(frame_visibility);
  }
}

void FrameView::UpdateRenderThrottlingStatus(bool hidden_for_throttling,
                                             bool subtree_throttled,
                                             bool recurse) {
  bool was_throttled = CanThrottleRendering();
  hidden_for_throttling_ = hidden_for_throttling;
  subtree_throttled_ = subtree_throttled;
  bool throttling_did_change = (was_throttled != CanThrottleRendering());
  if (throttling_did_change)
    RenderThrottlingStatusChanged();
  if (recurse) {
    for (Frame* child = GetFrame().Tree().FirstChild(); child;
         child = child->Tree().NextSibling()) {
      if (FrameView* child_view = child->View()) {
        child_view->UpdateRenderThrottlingStatus(
            child_view->IsHiddenForThrottling(),
            child_view->IsAttached() && CanThrottleRenderingForPropagation(),
            true);
      }
    }
  }
}

}  // namespace blink
