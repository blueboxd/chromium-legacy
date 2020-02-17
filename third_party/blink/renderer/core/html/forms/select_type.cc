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

#include "third_party/blink/public/strings/grit/blink_strings.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/popup_menu.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"

namespace blink {

class MenuListSelectType final : public SelectType {
 public:
  explicit MenuListSelectType(HTMLSelectElement& select) : SelectType(select) {}

  void DidSelectOption(HTMLOptionElement* element,
                       HTMLSelectElement::SelectOptionFlags flags,
                       bool should_update_popup) override;
  void UpdateTextStyle() override { UpdateTextStyleInternal(); }
  void UpdateTextStyleAndContent() override;

 private:
  String UpdateTextStyleInternal();
  void DidUpdateActiveOption(HTMLOptionElement* option);

  int ax_menulist_last_active_index_ = -1;
  bool has_updated_menulist_active_option_ = false;
};

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
  select_->option_style_ = option_style;

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
};

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

}  // namespace blink
