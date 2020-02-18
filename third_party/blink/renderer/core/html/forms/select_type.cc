/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2009, 2010, 2011 Apple Inc. All rights
 * reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/html/forms/select_type.h"

#include "build/build_config.h"
#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/events/gesture_event.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/popup_menu.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input/input_device_capabilities.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/page/autoscroll_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/spatial_navigation.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"

namespace blink {

namespace {

HTMLOptionElement* EventTargetOption(const Event& event) {
  return DynamicTo<HTMLOptionElement>(event.target()->ToNode());
}

}  // anonymous namespace

class MenuListSelectType final : public SelectType {
 public:
  explicit MenuListSelectType(HTMLSelectElement& select) : SelectType(select) {}

  bool DefaultEventHandler(const Event& event) override;
  void DidSelectOption(HTMLOptionElement* element,
                       HTMLSelectElement::SelectOptionFlags flags,
                       bool should_update_popup) override;
  void UpdateTextStyle() override { UpdateTextStyleInternal(); }
  void UpdateTextStyleAndContent() override;
  const ComputedStyle* OptionStyle() const override {
    return option_style_.get();
  }

 private:
  String UpdateTextStyleInternal();
  void DidUpdateActiveOption(HTMLOptionElement* option);

  scoped_refptr<const ComputedStyle> option_style_;
  int ax_menulist_last_active_index_ = -1;
  bool has_updated_menulist_active_option_ = false;
};

bool MenuListSelectType::DefaultEventHandler(const Event& event) {
  // We need to make the layout tree up-to-date to have GetLayoutObject() give
  // the correct result below. An author event handler may have set display to
  // some element to none which will cause a layout tree detach.
  select_->GetDocument().UpdateStyleAndLayoutTree();

  if (event.type() == event_type_names::kKeydown) {
    if (!select_->GetLayoutObject() || !event.IsKeyboardEvent())
      return false;

    const auto& key_event = ToKeyboardEvent(event);
    if (select_->ShouldOpenPopupForKeyDownEvent(key_event))
      return select_->HandlePopupOpenKeyboardEvent(event);

    // When using spatial navigation, we want to be able to navigate away
    // from the select element when the user hits any of the arrow keys,
    // instead of changing the selection.
    if (IsSpatialNavigationEnabled(select_->GetDocument().GetFrame())) {
      if (!select_->active_selection_state_)
        return false;
    }

    // The key handling below shouldn't be used for non spatial navigation
    // mode Mac
    if (LayoutTheme::GetTheme().PopsMenuByArrowKeys() &&
        !IsSpatialNavigationEnabled(select_->GetDocument().GetFrame()))
      return false;

    int ignore_modifiers = WebInputEvent::kShiftKey |
                           WebInputEvent::kControlKey | WebInputEvent::kAltKey |
                           WebInputEvent::kMetaKey;
    if (key_event.GetModifiers() & ignore_modifiers)
      return false;

    const String& key = key_event.key();
    bool handled = true;
    const HTMLSelectElement::ListItems& list_items = select_->GetListItems();
    HTMLOptionElement* option = select_->SelectedOption();
    int list_index = option ? option->ListIndex() : -1;

    if (key == "ArrowDown" || key == "ArrowRight") {
      option = select_->NextValidOption(list_index,
                                        HTMLSelectElement::kSkipForwards, 1);
    } else if (key == "ArrowUp" || key == "ArrowLeft") {
      option = select_->NextValidOption(list_index,
                                        HTMLSelectElement::kSkipBackwards, 1);
    } else if (key == "PageDown") {
      option = select_->NextValidOption(list_index,
                                        HTMLSelectElement::kSkipForwards, 3);
    } else if (key == "PageUp") {
      option = select_->NextValidOption(list_index,
                                        HTMLSelectElement::kSkipBackwards, 3);
    } else if (key == "Home") {
      option =
          select_->NextValidOption(-1, HTMLSelectElement::kSkipForwards, 1);
    } else if (key == "End") {
      option = select_->NextValidOption(list_items.size(),
                                        HTMLSelectElement::kSkipBackwards, 1);
    } else {
      handled = false;
    }

    if (handled && option) {
      select_->SelectOption(
          option, HTMLSelectElement::kDeselectOtherOptionsFlag |
                      HTMLSelectElement::kMakeOptionDirtyFlag |
                      HTMLSelectElement::kDispatchInputAndChangeEventFlag);
    }
    return handled;
  }

  if (event.type() == event_type_names::kKeypress) {
    if (!select_->GetLayoutObject() || !event.IsKeyboardEvent())
      return false;

    int key_code = ToKeyboardEvent(event).keyCode();
    if (key_code == ' ' &&
        IsSpatialNavigationEnabled(select_->GetDocument().GetFrame())) {
      // Use space to toggle arrow key handling for selection change or
      // spatial navigation.
      select_->active_selection_state_ = !select_->active_selection_state_;
      return true;
    }

    const auto& key_event = ToKeyboardEvent(event);
    if (select_->ShouldOpenPopupForKeyPressEvent(key_event))
      return select_->HandlePopupOpenKeyboardEvent(event);

    if (!LayoutTheme::GetTheme().PopsMenuByReturnKey() && key_code == '\r') {
      if (HTMLFormElement* form = select_->Form())
        form->SubmitImplicitly(event, false);
      select_->DispatchInputAndChangeEventForMenuList();
      return true;
    }
    return false;
  }

  const auto* mouse_event = DynamicTo<MouseEvent>(event);
  if (event.type() == event_type_names::kMousedown && mouse_event &&
      mouse_event->button() ==
          static_cast<int16_t>(WebPointerProperties::Button::kLeft)) {
    InputDeviceCapabilities* source_capabilities =
        select_->GetDocument()
            .domWindow()
            ->GetInputDeviceCapabilities()
            ->FiresTouchEvents(mouse_event->FromTouch());
    select_->focus(FocusParams(SelectionBehaviorOnFocus::kRestore,
                               mojom::blink::FocusType::kNone,
                               source_capabilities));
    if (select_->GetLayoutObject() && !will_be_destroyed_ &&
        !select_->IsDisabledFormControl()) {
      if (select_->PopupIsVisible()) {
        select_->HidePopup();
      } else {
        // Save the selection so it can be compared to the new selection
        // when we call onChange during selectOption, which gets called
        // from selectOptionByPopup, which gets called after the user
        // makes a selection from the menu.
        select_->SaveLastSelection();
        // TODO(lanwei): Will check if we need to add
        // InputDeviceCapabilities here when select menu list gets
        // focus, see https://crbug.com/476530.
        select_->ShowPopup();
      }
    }
    return true;
  }
  return false;
}

void MenuListSelectType::DidSelectOption(
    HTMLOptionElement* element,
    HTMLSelectElement::SelectOptionFlags flags,
    bool should_update_popup) {
  // Need to update last_on_change_option_ before UpdateFromElement().
  const bool should_dispatch_events =
      (flags & HTMLSelectElement::kDispatchInputAndChangeEventFlag) &&
      select_->last_on_change_option_ != element;
  select_->last_on_change_option_ = element;

  UpdateTextStyleAndContent();
  // PopupMenu::UpdateFromElement() posts an O(N) task.
  if (select_->PopupIsVisible() && should_update_popup)
    select_->popup_->UpdateFromElement(PopupMenu::kBySelectionChange);

  SelectType::DidSelectOption(element, flags, should_update_popup);

  if (should_dispatch_events) {
    select_->DispatchInputEvent();
    select_->DispatchChangeEvent();
  }
  if (select_->GetLayoutObject()) {
    // Need to check will_be_destroyed_ because event handlers might
    // disassociate |this| and select_.
    if (!will_be_destroyed_) {
      // DidUpdateActiveOption() is O(N) because of HTMLOptionElement::index().
      DidUpdateActiveOption(element);
    }
  }
}

String MenuListSelectType::UpdateTextStyleInternal() {
  HTMLOptionElement* option = select_->OptionToBeShown();
  String text = g_empty_string;
  const ComputedStyle* option_style = nullptr;

  if (select_->IsMultiple()) {
    unsigned selected_count = 0;
    HTMLOptionElement* selected_option_element = nullptr;
    for (auto* const option : select_->GetOptionList()) {
      if (option->Selected()) {
        if (++selected_count == 1)
          selected_option_element = option;
      }
    }

    if (selected_count == 1) {
      text = selected_option_element->TextIndentedToRespectGroupLabel();
      option_style = selected_option_element->GetComputedStyle();
    } else {
      Locale& locale = select_->GetLocale();
      String localized_number_string =
          locale.ConvertToLocalizedNumber(String::Number(selected_count));
      text = locale.QueryString(IDS_FORM_SELECT_MENU_LIST_TEXT,
                                localized_number_string);
      DCHECK(!option_style);
    }
  } else {
    if (option) {
      text = option->TextIndentedToRespectGroupLabel();
      option_style = option->GetComputedStyle();
    }
  }
  option_style_ = option_style;

  auto& inner_element = select_->InnerElement();
  const ComputedStyle* inner_style = inner_element.GetComputedStyle();
  if (inner_style && option_style &&
      ((option_style->Direction() != inner_style->Direction() ||
        option_style->GetUnicodeBidi() != inner_style->GetUnicodeBidi()))) {
    scoped_refptr<ComputedStyle> cloned_style =
        ComputedStyle::Clone(*inner_style);
    cloned_style->SetDirection(option_style->Direction());
    cloned_style->SetUnicodeBidi(option_style->GetUnicodeBidi());
    if (auto* inner_layout = inner_element.GetLayoutObject()) {
      inner_layout->SetModifiedStyleOutsideStyleRecalc(
          std::move(cloned_style), LayoutObject::ApplyStyleChanges::kYes);
    } else {
      inner_element.SetComputedStyle(std::move(cloned_style));
    }
  }
  if (select_->GetLayoutObject())
    DidUpdateActiveOption(option);

  return text.StripWhiteSpace();
}

void MenuListSelectType::UpdateTextStyleAndContent() {
  select_->InnerElement().firstChild()->setNodeValue(UpdateTextStyleInternal());
  // LayoutMenuList::ControlClipRect() depends on the content box size of
  // InnerElement.
  if (auto* box = select_->GetLayoutBox()) {
    box->SetNeedsPaintPropertyUpdate();
    if (auto* layer = box->Layer())
      layer->SetNeedsCompositingInputsUpdate();

    if (auto* cache = select_->GetDocument().ExistingAXObjectCache())
      cache->TextChanged(box);
  }
}

void MenuListSelectType::DidUpdateActiveOption(HTMLOptionElement* option) {
  Document& document = select_->GetDocument();
  if (!document.ExistingAXObjectCache())
    return;

  int option_index = option ? option->index() : -1;
  if (ax_menulist_last_active_index_ == option_index)
    return;
  ax_menulist_last_active_index_ = option_index;

  // We skip sending accessiblity notifications for the very first option,
  // otherwise we get extra focus and select events that are undesired.
  if (!has_updated_menulist_active_option_) {
    has_updated_menulist_active_option_ = true;
    return;
  }

  document.ExistingAXObjectCache()->HandleUpdateActiveMenuOption(
      select_->GetLayoutObject(), option_index);
}

// ============================================================================

class ListBoxSelectType final : public SelectType {
 public:
  explicit ListBoxSelectType(HTMLSelectElement& select) : SelectType(select) {}
  bool DefaultEventHandler(const Event& event) override;
};

bool ListBoxSelectType::DefaultEventHandler(const Event& event) {
  const auto* mouse_event = DynamicTo<MouseEvent>(event);
  if (event.type() == event_type_names::kGesturetap && event.IsGestureEvent()) {
    select_->focus();
    // Calling focus() may cause us to lose our layoutObject or change the
    // layoutObject type, in which case do not want to handle the event.
    if (!select_->GetLayoutObject() || will_be_destroyed_)
      return false;

    // Convert to coords relative to the list box if needed.
    const auto& gesture_event = ToGestureEvent(event);
    if (HTMLOptionElement* option = EventTargetOption(gesture_event)) {
      if (!select_->IsDisabledFormControl()) {
        select_->UpdateSelectedState(option, true, gesture_event.shiftKey());
        select_->ListBoxOnChange();
      }
      return true;
    }
    return false;
  }

  if (event.type() == event_type_names::kMousedown && mouse_event &&
      mouse_event->button() ==
          static_cast<int16_t>(WebPointerProperties::Button::kLeft)) {
    select_->focus();
    // Calling focus() may cause us to lose our layoutObject, in which case
    // do not want to handle the event.
    if (!select_->GetLayoutObject() || will_be_destroyed_ ||
        select_->IsDisabledFormControl())
      return false;

    // Convert to coords relative to the list box if needed.
    if (HTMLOptionElement* option = EventTargetOption(*mouse_event)) {
      if (!option->IsDisabledFormControl()) {
#if defined(OS_MACOSX)
        select_->UpdateSelectedState(option, mouse_event->metaKey(),
                                     mouse_event->shiftKey());
#else
        select_->UpdateSelectedState(option, mouse_event->ctrlKey(),
                                     mouse_event->shiftKey());
#endif
      }
      if (LocalFrame* frame = select_->GetDocument().GetFrame())
        frame->GetEventHandler().SetMouseDownMayStartAutoscroll();

      return true;
    }
    return false;
  }

  if (event.type() == event_type_names::kMousemove && mouse_event) {
    if (mouse_event->button() !=
            static_cast<int16_t>(WebPointerProperties::Button::kLeft) ||
        !mouse_event->ButtonDown())
      return false;

    if (auto* layout_object = select_->GetLayoutObject()) {
      layout_object->GetFrameView()->UpdateAllLifecyclePhasesExceptPaint(
          DocumentUpdateReason::kScroll);

      if (Page* page = select_->GetDocument().GetPage()) {
        page->GetAutoscrollController().StartAutoscrollForSelection(
            layout_object);
      }
    }
    // Mousedown didn't happen in this element.
    if (select_->last_on_change_selection_.IsEmpty())
      return false;

    if (HTMLOptionElement* option = EventTargetOption(*mouse_event)) {
      if (!select_->IsDisabledFormControl()) {
        if (select_->is_multiple_) {
          // Only extend selection if there is something selected.
          if (!select_->active_selection_anchor_)
            return false;

          select_->SetActiveSelectionEnd(option);
          select_->UpdateListBoxSelection(false);
        } else {
          select_->SetActiveSelectionAnchor(option);
          select_->SetActiveSelectionEnd(option);
          select_->UpdateListBoxSelection(true);
        }
      }
    }
    return false;
  }

  if (event.type() == event_type_names::kMouseup && mouse_event &&
      mouse_event->button() ==
          static_cast<int16_t>(WebPointerProperties::Button::kLeft) &&
      select_->GetLayoutObject()) {
    auto* page = select_->GetDocument().GetPage();
    if (page && page->GetAutoscrollController().AutoscrollInProgressFor(
                    select_->GetLayoutBox()))
      page->GetAutoscrollController().StopAutoscroll();
    else
      select_->HandleMouseRelease();
    return false;
  }

  if (event.type() == event_type_names::kKeydown) {
    const auto* keyboard_event = ToKeyboardEventOrNull(event);
    if (!keyboard_event)
      return false;
    const String& key = keyboard_event->key();

    bool handled = false;
    HTMLOptionElement* end_option = nullptr;
    if (!select_->active_selection_end_) {
      // Initialize the end index
      if (key == "ArrowDown" || key == "PageDown") {
        HTMLOptionElement* start_option = select_->LastSelectedOption();
        handled = true;
        if (key == "ArrowDown") {
          end_option = select_->NextSelectableOption(start_option);
        } else {
          end_option = select_->NextSelectableOptionPageAway(
              start_option, HTMLSelectElement::kSkipForwards);
        }
      } else if (key == "ArrowUp" || key == "PageUp") {
        HTMLOptionElement* start_option = select_->SelectedOption();
        handled = true;
        if (key == "ArrowUp") {
          end_option = select_->PreviousSelectableOption(start_option);
        } else {
          end_option = select_->NextSelectableOptionPageAway(
              start_option, HTMLSelectElement::kSkipBackwards);
        }
      }
    } else {
      // Set the end index based on the current end index.
      if (key == "ArrowDown") {
        end_option =
            select_->NextSelectableOption(select_->active_selection_end_.Get());
        handled = true;
      } else if (key == "ArrowUp") {
        end_option = select_->PreviousSelectableOption(
            select_->active_selection_end_.Get());
        handled = true;
      } else if (key == "PageDown") {
        end_option = select_->NextSelectableOptionPageAway(
            select_->active_selection_end_.Get(),
            HTMLSelectElement::kSkipForwards);
        handled = true;
      } else if (key == "PageUp") {
        end_option = select_->NextSelectableOptionPageAway(
            select_->active_selection_end_.Get(),
            HTMLSelectElement::kSkipBackwards);
        handled = true;
      }
    }
    if (key == "Home") {
      end_option = select_->FirstSelectableOption();
      handled = true;
    } else if (key == "End") {
      end_option = select_->LastSelectableOption();
      handled = true;
    }

    if (IsSpatialNavigationEnabled(select_->GetDocument().GetFrame())) {
      // Check if the selection moves to the boundary.
      if (key == "ArrowLeft" || key == "ArrowRight" ||
          ((key == "ArrowDown" || key == "ArrowUp") &&
           end_option == select_->active_selection_end_))
        return false;
    }

    bool is_control_key = false;
#if defined(OS_MACOSX)
    is_control_key = keyboard_event->metaKey();
#else
    is_control_key = keyboard_event->ctrlKey();
#endif

    if (select_->is_multiple_ && keyboard_event->keyCode() == ' ' &&
        is_control_key && select_->active_selection_end_) {
      // Use ctrl+space to toggle selection change.
      select_->ToggleSelection(*select_->active_selection_end_);
      return true;
    }

    if (end_option && handled) {
      // Save the selection so it can be compared to the new selection
      // when dispatching change events immediately after making the new
      // selection.
      select_->SaveLastSelection();

      select_->SetActiveSelectionEnd(end_option);

      select_->is_in_non_contiguous_selection_ =
          select_->is_multiple_ && is_control_key;
      bool select_new_item =
          !select_->is_multiple_ || keyboard_event->shiftKey() ||
          (!IsSpatialNavigationEnabled(select_->GetDocument().GetFrame()) &&
           !select_->is_in_non_contiguous_selection_);
      if (select_new_item)
        select_->active_selection_state_ = true;
      // If the anchor is uninitialized, or if we're going to deselect all
      // other options, then set the anchor index equal to the end index.
      bool deselect_others = !select_->is_multiple_ ||
                             (!keyboard_event->shiftKey() && select_new_item);
      if (!select_->active_selection_anchor_ || deselect_others) {
        if (deselect_others)
          select_->DeselectItemsWithoutValidation();
        select_->SetActiveSelectionAnchor(select_->active_selection_end_.Get());
      }

      select_->ScrollToOption(end_option);
      if (select_new_item || select_->is_in_non_contiguous_selection_) {
        if (select_new_item) {
          select_->UpdateListBoxSelection(deselect_others);
          select_->ListBoxOnChange();
        }
        select_->UpdateMultiSelectListBoxFocus();
      } else {
        select_->ScrollToSelection();
      }

      return true;
    }
    return false;
  }

  if (event.type() == event_type_names::kKeypress) {
    if (!event.IsKeyboardEvent())
      return false;
    int key_code = ToKeyboardEvent(event).keyCode();

    if (key_code == '\r') {
      if (HTMLFormElement* form = select_->Form())
        form->SubmitImplicitly(event, false);
      return true;
    } else if (select_->is_multiple_ && key_code == ' ' &&
               (IsSpatialNavigationEnabled(select_->GetDocument().GetFrame()) ||
                select_->is_in_non_contiguous_selection_)) {
      HTMLOptionElement* option = select_->active_selection_end_;
      // If there's no active selection,
      // act as if "ArrowDown" had been pressed.
      if (!option)
        option = select_->NextSelectableOption(select_->LastSelectedOption());
      if (option) {
        // Use space to toggle selection change.
        select_->ToggleSelection(*option);
        return true;
      }
    }
    return false;
  }
  return false;
}

// ============================================================================

SelectType::SelectType(HTMLSelectElement& select) : select_(select) {}

SelectType* SelectType::Create(HTMLSelectElement& select) {
  if (select.UsesMenuList())
    return MakeGarbageCollected<MenuListSelectType>(select);
  else
    return MakeGarbageCollected<ListBoxSelectType>(select);
}

void SelectType::WillBeDestroyed() {
  will_be_destroyed_ = true;
}

void SelectType::Trace(Visitor* visitor) {
  visitor->Trace(select_);
}

void SelectType::DidSelectOption(HTMLOptionElement*,
                                 HTMLSelectElement::SelectOptionFlags,
                                 bool) {
  select_->ScrollToSelection();
  select_->SetNeedsValidityCheck();
}

void SelectType::UpdateTextStyle() {}

void SelectType::UpdateTextStyleAndContent() {}

const ComputedStyle* SelectType::OptionStyle() const {
  NOTREACHED();
  return nullptr;
}

}  // namespace blink
