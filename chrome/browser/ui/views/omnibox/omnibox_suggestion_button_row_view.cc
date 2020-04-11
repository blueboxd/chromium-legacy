// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_suggestion_button_row_view.h"

#include "chrome/browser/ui/views/location_bar/selected_keyword_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_match_cell_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_contents_view.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_pedal.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/views/controls/focus_ring.h"

namespace {

views::MdTextButton* CreatePillButton(
    OmniboxSuggestionButtonRowView* button_row,
    const char* message) {
  views::MdTextButton* button = button_row->AddChildView(
      views::MdTextButton::Create(button_row, base::ASCIIToUTF16(message)));
  button->SetCornerRadius(16);
  button->SetVisible(false);
  return button;
}

size_t LayoutPillButton(views::MdTextButton* button,
                        size_t button_indent,
                        size_t suggestion_height) {
  gfx::Size button_size = button->GetPreferredSize();
  button->SetBounds(button_indent,
                    (suggestion_height - button_size.height()) / 2,
                    button_size.width(), button_size.height());
  button->SetVisible(true);
  // TODO(orinj): Determine and use the right gap between buttons.
  return button_indent + button_size.width() + 10;
}

}  // namespace

OmniboxSuggestionButtonRowView::OmniboxSuggestionButtonRowView(
    OmniboxPopupContentsView* popup_contents_view,
    int model_index)
    : popup_contents_view_(popup_contents_view), model_index_(model_index) {
  keyword_button_ = CreatePillButton(this, "Keyword search");
  pedal_button_ = CreatePillButton(this, "Pedal");
  // TODO(orinj): Use the real translated string table values here instead.
  tab_switch_button_ = CreatePillButton(this, "Switch to this tab");

  const auto make_predicate = [=](auto state) {
    return [=](View* view) {
      return view->GetVisible() &&
             model()->selection() ==
                 OmniboxPopupModel::Selection(model_index_, state);
    };
  };
  keyword_button_focus_ring_ = views::FocusRing::Install(keyword_button_);
  keyword_button_focus_ring_->SetHasFocusPredicate(
      make_predicate(OmniboxPopupModel::FOCUSED_BUTTON_KEYWORD));
  pedal_button_focus_ring_ = views::FocusRing::Install(pedal_button_);
  pedal_button_focus_ring_->SetHasFocusPredicate(
      make_predicate(OmniboxPopupModel::FOCUSED_BUTTON_PEDAL));
  tab_switch_button_focus_ring_ = views::FocusRing::Install(tab_switch_button_);
  tab_switch_button_focus_ring_->SetHasFocusPredicate(
      make_predicate(OmniboxPopupModel::FOCUSED_BUTTON_TAB_SWITCH));
}

OmniboxSuggestionButtonRowView::~OmniboxSuggestionButtonRowView() = default;

void OmniboxSuggestionButtonRowView::UpdateKeyword() {
  const OmniboxEditModel* edit_model = model()->edit_model();
  const base::string16& keyword = edit_model->keyword();
  const auto names = SelectedKeywordView::GetKeywordLabelNames(
      keyword, edit_model->client()->GetTemplateURLService());
  keyword_button_->SetText(names.full_name);
}

void OmniboxSuggestionButtonRowView::OnStyleRefresh() {
  keyword_button_focus_ring_->SchedulePaint();
  pedal_button_focus_ring_->SchedulePaint();
  tab_switch_button_focus_ring_->SchedulePaint();
}

void OmniboxSuggestionButtonRowView::ButtonPressed(views::Button* button,
                                                   const ui::Event& event) {
  if (button == tab_switch_button_) {
    popup_contents_view_->OpenMatch(
        model_index_, WindowOpenDisposition::SWITCH_TO_TAB, event.time_stamp());
  } else if (button == keyword_button_) {
    // TODO(orinj): Clear out existing suggestions, particularly this one, as
    // once we AcceptKeyword, we are really in a new scope state and holding
    // onto old suggestions is confusing and error prone. Without this check,
    // a second click of the button violates assumptions in |AcceptKeyword|.
    if (model()->edit_model()->is_keyword_hint()) {
      auto method = metrics::OmniboxEventProto::INVALID;
      if (event.IsKeyEvent()) {
        method = metrics::OmniboxEventProto::KEYBOARD_SHORTCUT;
      } else if (event.IsMouseEvent()) {
        method = metrics::OmniboxEventProto::CLICK_HINT_VIEW;
      } else if (event.IsGestureEvent()) {
        method = metrics::OmniboxEventProto::TAP_HINT_VIEW;
      }
      DCHECK_NE(method, metrics::OmniboxEventProto::INVALID);
      model()->edit_model()->AcceptKeyword(method);
    }
  } else if (button == pedal_button_) {
    DCHECK(match().pedal);
    // Pedal action intent means we execute the match instead of opening it.
    model()->edit_model()->ExecutePedal(match(), event.time_stamp());
  }
}

void OmniboxSuggestionButtonRowView::Layout() {
  views::View::Layout();

  // TODO(orinj): Rework layout with a layout manager. For now this depends
  // on bounds already being set by parent.
  const int suggestion_height = height();

  const auto check_state = [=](auto state) {
    return model()->IsSelectionAvailable(
        OmniboxPopupModel::Selection(model_index_, state));
  };
  int start_indent = OmniboxMatchCellView::GetTextIndent();
  // This button_indent strictly increases with each button added.
  int button_indent = start_indent;
  if (check_state(OmniboxPopupModel::FOCUSED_BUTTON_KEYWORD)) {
    button_indent =
        LayoutPillButton(keyword_button_, button_indent, suggestion_height);
  } else if (keyword_button_->GetVisible()) {
    // Setting visibility does lots of work, even if not changing.
    keyword_button_->SetVisible(false);
  }
  if (check_state(OmniboxPopupModel::FOCUSED_BUTTON_PEDAL)) {
    pedal_button_->SetText(match().pedal->GetLabelStrings().hint);
    button_indent =
        LayoutPillButton(pedal_button_, button_indent, suggestion_height);
  } else if (pedal_button_->GetVisible()) {
    pedal_button_->SetVisible(false);
  }
  if (check_state(OmniboxPopupModel::FOCUSED_BUTTON_TAB_SWITCH)) {
    button_indent =
        LayoutPillButton(tab_switch_button_, button_indent, suggestion_height);
  } else if (tab_switch_button_->GetVisible()) {
    tab_switch_button_->SetVisible(false);
  }

  if (button_indent != start_indent) {
    SetVisible(true);
  } else if (GetVisible()) {
    SetVisible(false);
  }

  // TODO(orinj): Migrate to BoxLayout or FlexLayout. Ideally we don't do any
  // more layouts ourselves. Also, check visibility management in result view.
}

const OmniboxPopupModel* OmniboxSuggestionButtonRowView::model() const {
  return popup_contents_view_->model();
}

const AutocompleteMatch& OmniboxSuggestionButtonRowView::match() const {
  return model()->result().match_at(model_index_);
}
