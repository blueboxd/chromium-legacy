// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/accessibility/arc_accessibility_util.h"
#include "chrome/browser/chromeos/arc/accessibility/accessibility_info_data_wrapper.h"

#include "components/arc/mojom/accessibility_helper.mojom.h"
#include "ui/accessibility/ax_enums.mojom.h"

namespace arc {

using AXActionType = mojom::AccessibilityActionType;
using AXBooleanProperty = mojom::AccessibilityBooleanProperty;
using AXIntListProperty = mojom::AccessibilityIntListProperty;
using AXNodeInfoData = mojom::AccessibilityNodeInfoData;
using AXStringProperty = mojom::AccessibilityStringProperty;

ax::mojom::Event ToAXEvent(mojom::AccessibilityEventType arc_event_type,
                           AccessibilityInfoDataWrapper* source_node,
                           AccessibilityInfoDataWrapper* focused_node) {
  switch (arc_event_type) {
    case mojom::AccessibilityEventType::VIEW_FOCUSED:
    case mojom::AccessibilityEventType::VIEW_ACCESSIBILITY_FOCUSED:
      return ax::mojom::Event::kFocus;
    case mojom::AccessibilityEventType::VIEW_CLICKED:
    case mojom::AccessibilityEventType::VIEW_LONG_CLICKED:
      return ax::mojom::Event::kClicked;
    case mojom::AccessibilityEventType::VIEW_TEXT_CHANGED:
      return ax::mojom::Event::kTextChanged;
    case mojom::AccessibilityEventType::VIEW_TEXT_SELECTION_CHANGED:
      return ax::mojom::Event::kTextSelectionChanged;
    case mojom::AccessibilityEventType::WINDOW_STATE_CHANGED: {
      if (focused_node)
        return ax::mojom::Event::kFocus;
      else
        return ax::mojom::Event::kLayoutComplete;
    }
    case mojom::AccessibilityEventType::NOTIFICATION_STATE_CHANGED:
    case mojom::AccessibilityEventType::WINDOW_CONTENT_CHANGED:
    case mojom::AccessibilityEventType::WINDOWS_CHANGED:
      return ax::mojom::Event::kLayoutComplete;
    case mojom::AccessibilityEventType::VIEW_HOVER_ENTER:
      return ax::mojom::Event::kHover;
    case mojom::AccessibilityEventType::ANNOUNCEMENT: {
      // NOTE: Announcement event is handled in
      // ArcAccessibilityHelperBridge::OnAccessibilityEvent.
      NOTREACHED();
      break;
    }
    case mojom::AccessibilityEventType::VIEW_SCROLLED:
      return ax::mojom::Event::kScrollPositionChanged;
    case mojom::AccessibilityEventType::VIEW_SELECTED: {
      // VIEW_SELECTED event is not selection event in Chrome.
      // See the comment on AXTreeSourceArc::NotifyAccessibilityEvent.
      if (source_node && source_node->IsNode() &&
          source_node->GetNode()->range_info) {
        return ax::mojom::Event::kValueChanged;
      } else {
        return ax::mojom::Event::kFocus;
      }
    }
    case mojom::AccessibilityEventType::VIEW_HOVER_EXIT:
    case mojom::AccessibilityEventType::TOUCH_EXPLORATION_GESTURE_START:
    case mojom::AccessibilityEventType::TOUCH_EXPLORATION_GESTURE_END:
    case mojom::AccessibilityEventType::
        VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY:
    case mojom::AccessibilityEventType::GESTURE_DETECTION_START:
    case mojom::AccessibilityEventType::GESTURE_DETECTION_END:
    case mojom::AccessibilityEventType::TOUCH_INTERACTION_START:
    case mojom::AccessibilityEventType::TOUCH_INTERACTION_END:
    case mojom::AccessibilityEventType::VIEW_CONTEXT_CLICKED:
    case mojom::AccessibilityEventType::ASSIST_READING_CONTEXT:
      return ax::mojom::Event::kChildrenChanged;
    default:
      return ax::mojom::Event::kChildrenChanged;
  }
  return ax::mojom::Event::kChildrenChanged;
}

base::Optional<mojom::AccessibilityActionType> ConvertToAndroidAction(
    ax::mojom::Action action) {
  switch (action) {
    case ax::mojom::Action::kDoDefault:
      return arc::mojom::AccessibilityActionType::CLICK;
    case ax::mojom::Action::kFocus:
      return arc::mojom::AccessibilityActionType::ACCESSIBILITY_FOCUS;
    case ax::mojom::Action::kScrollToMakeVisible:
      return arc::mojom::AccessibilityActionType::SHOW_ON_SCREEN;
    case ax::mojom::Action::kScrollBackward:
      return arc::mojom::AccessibilityActionType::SCROLL_BACKWARD;
    case ax::mojom::Action::kScrollForward:
      return arc::mojom::AccessibilityActionType::SCROLL_FORWARD;
    case ax::mojom::Action::kScrollUp:
      return arc::mojom::AccessibilityActionType::SCROLL_UP;
    case ax::mojom::Action::kScrollDown:
      return arc::mojom::AccessibilityActionType::SCROLL_DOWN;
    case ax::mojom::Action::kScrollLeft:
      return arc::mojom::AccessibilityActionType::SCROLL_LEFT;
    case ax::mojom::Action::kScrollRight:
      return arc::mojom::AccessibilityActionType::SCROLL_RIGHT;
    case ax::mojom::Action::kCustomAction:
      return arc::mojom::AccessibilityActionType::CUSTOM_ACTION;
    case ax::mojom::Action::kSetAccessibilityFocus:
      return arc::mojom::AccessibilityActionType::ACCESSIBILITY_FOCUS;
    case ax::mojom::Action::kClearAccessibilityFocus:
      return arc::mojom::AccessibilityActionType::CLEAR_ACCESSIBILITY_FOCUS;
    case ax::mojom::Action::kGetTextLocation:
      return arc::mojom::AccessibilityActionType::GET_TEXT_LOCATION;
    case ax::mojom::Action::kShowTooltip:
      return arc::mojom::AccessibilityActionType::SHOW_TOOLTIP;
    case ax::mojom::Action::kHideTooltip:
      return arc::mojom::AccessibilityActionType::HIDE_TOOLTIP;
    case ax::mojom::Action::kCollapse:
      return arc::mojom::AccessibilityActionType::COLLAPSE;
    case ax::mojom::Action::kExpand:
      return arc::mojom::AccessibilityActionType::EXPAND;
    default:
      return base::nullopt;
  }
}

std::string ToLiveStatusString(mojom::AccessibilityLiveRegionType type) {
  switch (type) {
    case mojom::AccessibilityLiveRegionType::NONE:
      return "none";
    case mojom::AccessibilityLiveRegionType::POLITE:
      return "polite";
    case mojom::AccessibilityLiveRegionType::ASSERTIVE:
      return "assertive";
    default:
      NOTREACHED();
  }
  return std::string();  // Dummy.
}

bool IsImportantInAndroid(AXNodeInfoData* node) {
  if (!node)
    return false;

  return node->is_virtual_node ||
         GetBooleanProperty(node, AXBooleanProperty::IMPORTANCE);
}

bool HasImportantProperty(AXNodeInfoData* node) {
  if (!node)
    return false;

  // These properties are used to compute accessibility name in
  // AccessibilityNodeInfoDataWrapper.
  // TODO(hirokisato): Also check LABELED_BY.
  if (HasNonEmptyStringProperty(node, AXStringProperty::CONTENT_DESCRIPTION) ||
      HasNonEmptyStringProperty(node, AXStringProperty::TEXT) ||
      HasNonEmptyStringProperty(node, AXStringProperty::PANE_TITLE) ||
      HasNonEmptyStringProperty(node, AXStringProperty::HINT_TEXT)) {
    return true;
  }

  // These properties are sorted in the same order of mojom file.
  if (GetBooleanProperty(node, AXBooleanProperty::CHECKABLE) ||
      GetBooleanProperty(node, AXBooleanProperty::FOCUSABLE) ||
      GetBooleanProperty(node, AXBooleanProperty::SELECTED) ||
      GetBooleanProperty(node, AXBooleanProperty::EDITABLE)) {
    return true;
  }

  if (HasStandardAction(node, AXActionType::FOCUS) ||
      HasStandardAction(node, AXActionType::CLEAR_FOCUS) ||
      HasStandardAction(node, AXActionType::CLICK)) {
    return true;
  }

  // TODO(hirokisato): Consider to check ui::IsControl(role).
  return false;
}

bool HasStandardAction(AXNodeInfoData* node, AXActionType action) {
  if (!node || !node->int_list_properties)
    return false;

  auto itr =
      node->int_list_properties->find(AXIntListProperty::STANDARD_ACTION_IDS);
  if (itr == node->int_list_properties->end())
    return false;

  for (const auto supported_action : itr->second) {
    if (static_cast<AXActionType>(supported_action) == action)
      return true;
  }
  return false;
}

}  // namespace arc
