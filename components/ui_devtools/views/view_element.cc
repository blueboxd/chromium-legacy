// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/views/view_element.h"

#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/ui_devtools/Protocol.h"
#include "components/ui_devtools/ui_element_delegate.h"
#include "components/ui_devtools/views/element_utility.h"
#include "ui/views/widget/widget.h"

namespace ui_devtools {

ViewElement::ViewElement(views::View* view,
                         UIElementDelegate* ui_element_delegate,
                         UIElement* parent)
    : UIElement(UIElementType::VIEW, ui_element_delegate, parent), view_(view) {
  view_->AddObserver(this);
}

ViewElement::~ViewElement() {
  view_->RemoveObserver(this);
}

void ViewElement::OnChildViewRemoved(views::View* parent, views::View* view) {
  DCHECK_EQ(parent, view_);
  auto iter = std::find_if(
      children().begin(), children().end(), [view](UIElement* child) {
        return view ==
               UIElement::GetBackingElement<views::View, ViewElement>(child);
      });
  DCHECK(iter != children().end());
  UIElement* child_element = *iter;
  RemoveChild(child_element);
  delete child_element;
}

void ViewElement::OnChildViewAdded(views::View* parent, views::View* view) {
  DCHECK_EQ(parent, view_);
  AddChild(new ViewElement(view, delegate(), this));
}

void ViewElement::OnChildViewReordered(views::View* parent, views::View* view) {
  DCHECK_EQ(parent, view_);
  auto iter = std::find_if(
      children().begin(), children().end(), [view](UIElement* child) {
        return view ==
               UIElement::GetBackingElement<views::View, ViewElement>(child);
      });
  DCHECK(iter != children().end());
  UIElement* child_element = *iter;
  ReorderChild(child_element, parent->GetIndexOf(view));
}

void ViewElement::OnViewBoundsChanged(views::View* view) {
  DCHECK_EQ(view_, view);
  delegate()->OnUIElementBoundsChanged(this);
}

std::vector<std::pair<std::string, std::string>>
ViewElement::GetCustomProperties() const {
  std::vector<std::pair<std::string, std::string>> ret;

  views::metadata::ClassMetaData* metadata = view_->GetClassMetaData();
  for (views::metadata::MemberMetaDataBase* member : *metadata) {
    ret.emplace_back(member->member_name(),
                     base::UTF16ToUTF8(member->GetValueAsString(view_)));
  }

  ret.emplace_back("is-drawn", view_->IsDrawn() ? "true" : "false");
  ui::Layer* layer = view_->layer();
  if (layer) {
    ret.emplace_back("layer", layer->name());
    AppendLayerProperties(layer, &ret);
  }

  base::string16 description = view_->GetTooltipText(gfx::Point());
  if (!description.empty())
    ret.emplace_back("tooltip", base::UTF16ToUTF8(description));

  return ret;
}

void ViewElement::GetBounds(gfx::Rect* bounds) const {
  *bounds = view_->bounds();
}

void ViewElement::SetBounds(const gfx::Rect& bounds) {
  view_->SetBoundsRect(bounds);
}

void ViewElement::GetVisible(bool* visible) const {
  // Visibility information should be directly retrieved from View's metadata,
  // no need for this function any more.
  NOTREACHED();
}

void ViewElement::SetVisible(bool visible) {
  // Intentional No-op.
}

bool ViewElement::SetPropertiesFromString(const std::string& text) {
  std::vector<std::string> tokens = base::SplitString(
      text, ":;", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  if (tokens.size() == 0UL)
    return false;

  for (size_t i = 0; i < tokens.size() - 1; i += 2) {
    const std::string& property_name = tokens.at(i);
    const std::string& property_value = base::ToLowerASCII(tokens.at(i + 1));

    views::metadata::ClassMetaData* metadata = view_->GetClassMetaData();
    views::metadata::MemberMetaDataBase* member =
        metadata->FindMemberData(property_name);
    if (!member) {
      DLOG(ERROR) << "UI DevTools: Can not find property " << property_name
                  << " in MetaData.";
      continue;
    }

    // Since DevTools frontend doesn't check the value, we do a sanity check
    // based on its type here.
    if (member->member_type() == "bool") {
      if (property_value != "true" && property_value != "false") {
        // Ignore the value.
        continue;
      }
    }

    member->SetValueAsString(view_, base::UTF8ToUTF16(property_value));
  }

  return true;
}

std::vector<std::string> ViewElement::GetAttributes() const {
  // TODO(lgrey): Change name to class after updating tests.
  return {"name", view_->GetClassName()};
}

std::pair<gfx::NativeWindow, gfx::Rect>
ViewElement::GetNodeWindowAndScreenBounds() const {
  return std::make_pair(view_->GetWidget()->GetNativeWindow(),
                        view_->GetBoundsInScreen());
}

// static
views::View* ViewElement::From(const UIElement* element) {
  DCHECK_EQ(UIElementType::VIEW, element->type());
  return static_cast<const ViewElement*>(element)->view_;
}

template <>
int UIElement::FindUIElementIdForBackendElement<views::View>(
    views::View* element) const {
  if (type_ == UIElementType::VIEW &&
      UIElement::GetBackingElement<views::View, ViewElement>(this) == element) {
    return node_id_;
  }
  for (auto* child : children_) {
    int ui_element_id = child->FindUIElementIdForBackendElement(element);
    if (ui_element_id)
      return ui_element_id;
  }
  return 0;
}

}  // namespace ui_devtools
