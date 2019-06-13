// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/input/fallback_cursor_event_manager.h"

#include "third_party/blink/public/platform/web_mouse_event.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/events/web_input_event_conversion.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input/scroll_manager.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_request.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/platform/geometry/int_point.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

namespace {

const float kScrollAreaRatio = 0.3f;

LocalFrame* FrameOfNode(const Node& node) {
  return node.GetDocument().GetFrame();
}

Node* ParentNode(const Node& node) {
  if (node.IsDocumentNode()) {
    FrameOwner* frame_owner = FrameOfNode(node)->Owner();
    DCHECK(frame_owner->IsLocal());
    return DynamicTo<HTMLFrameOwnerElement>(frame_owner);
  }

  return node.parentNode();
}

HitTestResult HitTest(LayoutView* layout_view, const IntPoint& point_in_root) {
  HitTestRequest request(HitTestRequest::kReadOnly |
                         HitTestRequest::kAllowChildFrameContent);
  HitTestLocation location(point_in_root);
  HitTestResult result(request, location);
  layout_view->HitTest(location, result);

  return result;
}

bool CanScrollAnyDirection(const ScrollableArea& scrollable) {
  IntSize scroll_dimensions =
      scrollable.MaximumScrollOffsetInt() - scrollable.MinimumScrollOffsetInt();
  return !scroll_dimensions.IsZero();
}

IntSize ScrollableAreaClipSizeInRootFrame(const ScrollableArea& scrollable) {
  LayoutBox* box = scrollable.GetLayoutBox();
  DCHECK(box);
  LocalFrameView* view = box->GetFrameView();
  DCHECK(view);

  PhysicalRect rect(scrollable.VisibleContentRect(blink::kIncludeScrollbars));
  rect = view->DocumentToFrame(rect);
  return view->ConvertToRootFrame(EnclosedIntRect(FloatRect(rect))).Size();
}

IntPoint RootFrameLocationToScrollable(const IntPoint& location_in_root_frame,
                                       const ScrollableArea& scrollable) {
  LayoutBox* box = scrollable.GetLayoutBox();
  DCHECK(box);
  LocalFrameView* view = box->GetFrameView();

  DCHECK(view);

  IntPoint location_in_frame =
      view->ConvertFromRootFrame(location_in_root_frame);

  if (&scrollable == view->GetScrollableArea())
    return location_in_frame;

  DCHECK(scrollable.IsPaintLayerScrollableArea());

  IntPoint location_in_box =
      view->ConvertToLayoutObject(*box, location_in_frame);
  location_in_box.Move(-box->BorderLeft().ToInt(), -box->BorderTop().ToInt());
  return location_in_box;
}

}  // namespace

FallbackCursorEventManager::FallbackCursorEventManager(LocalFrame& root_frame)
    : root_frame_(root_frame) {
  DCHECK(root_frame.IsMainFrame());
  ResetCurrentScrollable();
}

void FallbackCursorEventManager::Trace(blink::Visitor* visitor) {
  visitor->Trace(root_frame_);
  visitor->Trace(current_node_);
}

void FallbackCursorEventManager::ResetCurrentScrollable() {
  current_node_.Clear();
}

// Check if current scrolling ScrollableArea is still valid and reset it if not.
void FallbackCursorEventManager::InvalidateCurrentScrollableIfNeeded() {
  if (!current_node_)
    return;

  if (!current_node_->isConnected() ||
      !current_node_->GetDocument().GetFrame()) {
    ResetCurrentScrollable();
  }
  ScrollableArea* current_scrollable = CurrentScrollingScrollableArea();
  if (!current_scrollable || !CanScrollAnyDirection(*current_scrollable))
    ResetCurrentScrollable();
}

ScrollableArea* FallbackCursorEventManager::CurrentScrollingScrollableArea() {
  LocalFrame* current_frame = CurrentScrollingFrame();
  Node* current_node = CurrentScrollingNode();

  if (current_node->IsDocumentNode())
    return current_frame->View()->GetScrollableArea();

  auto* layout_object = current_node->GetLayoutObject();
  if (!layout_object || !layout_object->IsBox())
    return nullptr;

  return ToLayoutBox(current_node->GetLayoutObject())->GetScrollableArea();
}

Node* FallbackCursorEventManager::CurrentScrollingNode() {
  if (!current_node_)
    return root_frame_->GetDocument();

  return current_node_.Get();
}

LocalFrame* FallbackCursorEventManager::CurrentScrollingFrame() {
  if (!current_node_)
    return root_frame_;

  return FrameOfNode(*current_node_.Get());
}

bool FallbackCursorEventManager::ShouldLock(
    Direction d,
    const ScrollableArea& scrollable,
    const IntSize& node_size,
    const IntPoint& cursor_location_in_node) {
  // Check can scroll in direction, if not should not lock this direction.
  IntSize current_offset = scrollable.ScrollOffsetInt();
  IntSize min_offset = scrollable.MinimumScrollOffsetInt();
  IntSize max_offset = scrollable.MaximumScrollOffsetInt();

  switch (d) {
    case Direction::kLeft:
      if (current_offset.Width() <= min_offset.Width())
        return false;
      break;
    case Direction::kRight:
      if (current_offset.Width() >= max_offset.Width())
        return false;
      break;
    case Direction::kUp:
      if (current_offset.Height() <= min_offset.Height())
        return false;
      break;
    case Direction::kDown:
      if (current_offset.Height() >= max_offset.Height())
        return false;
      break;
    default:
      NOTREACHED();
  }

  // Check if cursor located in scroll area.
  switch (d) {
    case Direction::kLeft:
      if (cursor_location_in_node.X() < node_size.Width() * kScrollAreaRatio) {
        return true;
      }
      break;
    case Direction::kRight:
      if (cursor_location_in_node.X() >
          node_size.Width() * (1 - kScrollAreaRatio)) {
        return true;
      }
      break;
    case Direction::kUp:
      if (cursor_location_in_node.Y() < node_size.Height() * kScrollAreaRatio) {
        return true;
      }
      break;
    case Direction::kDown:
      if (cursor_location_in_node.Y() >
          node_size.Height() * (1 - kScrollAreaRatio)) {
        return true;
      }
      break;
    default:
      NOTREACHED();
  }

  return false;
}

void FallbackCursorEventManager::LockCursor(bool left,
                                            bool right,
                                            bool up,
                                            bool down) {
  root_frame_->GetChromeClient().FallbackCursorModeLockCursor(
      root_frame_.Get(), left, right, up, down);
}

void FallbackCursorEventManager::SetCursorVisibility(bool visible) {
  root_frame_->GetChromeClient().FallbackCursorModeSetCursorVisibility(
      root_frame_.Get(), visible);
}

void FallbackCursorEventManager::ComputeLockCursor(
    const IntPoint& location_in_root_frame) {
  ScrollableArea* scrollable = CurrentScrollingScrollableArea();

  DCHECK(scrollable);
  IntSize scrollable_clip_size_in_root_frame =
      ScrollableAreaClipSizeInRootFrame(*scrollable);
  IntPoint location_in_scrollable =
      RootFrameLocationToScrollable(location_in_root_frame, *scrollable);

  bool left =
      ShouldLock(Direction::kLeft, *scrollable,
                 scrollable_clip_size_in_root_frame, location_in_scrollable);
  bool right =
      ShouldLock(Direction::kRight, *scrollable,
                 scrollable_clip_size_in_root_frame, location_in_scrollable);
  bool up =
      ShouldLock(Direction::kUp, *scrollable,
                 scrollable_clip_size_in_root_frame, location_in_scrollable);
  bool down =
      ShouldLock(Direction::kDown, *scrollable,
                 scrollable_clip_size_in_root_frame, location_in_scrollable);

  LockCursor(left, right, up, down);
}

void FallbackCursorEventManager::HandleMouseMoveEvent(const WebMouseEvent& e) {
  DCHECK(RuntimeEnabledFeatures::FallbackCursorModeEnabled());
  // TODO(crbug.com/953393): reenable after bug is fixed.
  // DCHECK(is_fallback_cursor_mode_on_);

  InvalidateCurrentScrollableIfNeeded();
  ScrollableArea* scrollable = CurrentScrollingScrollableArea();

  DCHECK(scrollable);

  IntPoint location_in_root_frame{e.PositionInRootFrame().x,
                                  e.PositionInRootFrame().y};

  IntSize scrollable_clip_size_in_root_frame =
      ScrollableAreaClipSizeInRootFrame(*scrollable);
  IntPoint location_in_scrollable =
      RootFrameLocationToScrollable(location_in_root_frame, *scrollable);

  // Check if mouse out of current node.
  IntRect rect = IntRect(IntPoint(), scrollable_clip_size_in_root_frame);
  if (!rect.Contains(location_in_scrollable))
    ResetCurrentScrollable();

  ComputeLockCursor(location_in_root_frame);
}

void FallbackCursorEventManager::HandleMousePressEvent(const WebMouseEvent& e) {
  DCHECK(RuntimeEnabledFeatures::FallbackCursorModeEnabled());
  // TODO(crbug.com/953393): reenable after bug is fixed.
  // DCHECK(is_fallback_cursor_mode_on_);

  ResetCurrentScrollable();

  // Re hit test since we need a hit test with child frame.
  IntPoint location{e.PositionInRootFrame().x, e.PositionInRootFrame().y};
  HitTestResult hit_test_result =
      HitTest(root_frame_->GetDocument()->GetLayoutView(), location);
  Node* node = hit_test_result.InnerNode();

  // Click on input boxes or media node should hide the cursor.
  if (HasEditableStyle(*node) || node->IsMediaElement()) {
    SetCursorVisibility(false);
    return;
  }

  for (; node; node = ParentNode(*node)) {
    ScrollableArea* scrollable = nullptr;
    if (node->IsDocumentNode()) {
      LocalFrame* current_frame = FrameOfNode(*node);
      DCHECK(current_frame);
      scrollable = current_frame->View()->GetScrollableArea();
    } else {
      auto* layout_object = node->GetLayoutObject();
      if (!layout_object || !layout_object->IsBox()) {
        continue;
      }

      LayoutBox* box = ToLayoutBox(layout_object);
      if (!box->CanBeScrolledAndHasScrollableArea()) {
        continue;
      }
      scrollable = box->GetScrollableArea();
    }

    DCHECK(scrollable);
    if (!CanScrollAnyDirection(*scrollable))
      continue;

    // Found scrollable
    break;
  }

  current_node_ = node;
}

Element* FallbackCursorEventManager::GetFocusedElement() const {
  DCHECK(root_frame_->GetPage());
  LocalFrame* frame =
      root_frame_->GetPage()->GetFocusController().FocusedFrame();
  if (!frame || !frame->GetDocument())
    return nullptr;

  return frame->GetDocument()->FocusedElement();
}

bool FallbackCursorEventManager::HandleKeyBackEvent() {
  DCHECK(RuntimeEnabledFeatures::FallbackCursorModeEnabled());

  if (!is_fallback_cursor_mode_on_)
    return false;

  SetCursorVisibility(true);
  if (Element* focused_element = GetFocusedElement()) {
    focused_element->blur();
    return true;
  }

  ResetCurrentScrollable();
  return true;
}

void FallbackCursorEventManager::SetIsFallbackCursorModeOn(bool is_on) {
  is_fallback_cursor_mode_on_ = is_on;
  DCHECK(root_frame_->GetPage());
  root_frame_->GetPage()->GetSettings().SetSpatialNavigationEnabled(!is_on);
}

}  // namespace blink
