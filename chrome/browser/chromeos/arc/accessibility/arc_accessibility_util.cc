// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/accessibility/arc_accessibility_util.h"

#include "components/arc/mojom/accessibility_helper.mojom.h"
#include "ui/accessibility/ax_enums.mojom.h"

namespace arc {

using AXActionType = mojom::AccessibilityActionType;
using AXBooleanProperty = mojom::AccessibilityBooleanProperty;
using AXIntListProperty = mojom::AccessibilityIntListProperty;
using AXNodeInfoData = mojom::AccessibilityNodeInfoData;
using AXStringProperty = mojom::AccessibilityStringProperty;

ax::mojom::Event ToAXEvent(
    mojom::AccessibilityEventType arc_event_type,
    mojom::AccessibilityNodeInfoData* focused_node_info_data) {
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
      if (focused_node_info_data)
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
      // In Android, VIEW_SELECTED event is fired in the two cases below:
      // 1. Changing a value in ProgressBar or TimePicker.
      //    (this usage is NOT documented)
      // 2. Selecting an item in the context of an AdapterView.
      //    (officially documented in Android Developer doc below)
      //    https://developer.android.com/reference/android/view/accessibility/AccessibilityEvent#TYPE_VIEW_SELECTED
      if (focused_node_info_data && focused_node_info_data->range_info)
        return ax::mojom::Event::kValueChanged;
      else
        return ax::mojom::Event::kSelection;
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
    default:
      return base::nullopt;
  }
}

bool IsImportantInAndroid(AXNodeInfoData* node) {
  if (!node)
    return false;

  if (GetBooleanProperty(node, AXBooleanProperty::IMPORTANCE))
    return true;

  // WebView and its child nodes do not have accessibility importance set.
  // This logic can be removed once the change in crrev/c/1890402 landed
  // in all ARC containers.
  std::vector<int32_t> standard_action_ids;
  if (GetProperty(node->int_list_properties,
                  AXIntListProperty::STANDARD_ACTION_IDS,
                  &standard_action_ids)) {
    for (const int32_t id : standard_action_ids) {
      switch (static_cast<AXActionType>(id)) {
        case AXActionType::NEXT_HTML_ELEMENT:
        case AXActionType::PREVIOUS_HTML_ELEMENT:
          return true;
        default:
          // unused.
          break;
      }
    }
  }

  return false;
}

bool HasImportantProperty(AXNodeInfoData* node) {
  if (!node)
    return false;

  std::string prop;
  if (HasNonEmptyStringProperty(node, AXStringProperty::CONTENT_DESCRIPTION) ||
      HasNonEmptyStringProperty(node, AXStringProperty::TEXT) ||
      HasNonEmptyStringProperty(node, AXStringProperty::PANE_TITLE) ||
      HasNonEmptyStringProperty(node, AXStringProperty::HINT_TEXT))
    return true;

  if (GetBooleanProperty(node, AXBooleanProperty::EDITABLE) ||
      GetBooleanProperty(node, AXBooleanProperty::CHECKABLE) ||
      GetBooleanProperty(node, AXBooleanProperty::SELECTED))
    return true;

  std::vector<int32_t> standard_action_ids;
  if (GetProperty(node->int_list_properties,
                  AXIntListProperty::STANDARD_ACTION_IDS,
                  &standard_action_ids)) {
    for (const int32_t id : standard_action_ids) {
      switch (static_cast<AXActionType>(id)) {
        case AXActionType::CLICK:
        case AXActionType::FOCUS:
          return true;
        default:
          // unused.
          break;
      }
    }
  }

  // TODO(hirokisato) Also check LABELED_BY and ui::IsControl(role)
  return false;
}

bool GetBooleanProperty(mojom::AccessibilityNodeInfoData* node,
                        mojom::AccessibilityBooleanProperty prop) {
  if (!node || !node->boolean_properties)
    return false;

  auto it = node->boolean_properties->find(prop);
  if (it == node->boolean_properties->end())
    return false;

  return it->second;
}

}  // namespace arc
