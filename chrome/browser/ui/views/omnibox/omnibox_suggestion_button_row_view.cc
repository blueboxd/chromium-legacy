// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_suggestion_button_row_view.h"

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/location_bar/selected_keyword_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_match_cell_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_contents_view.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_pedal.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

class OmniboxSuggestionRowButton : public views::MdTextButton {
 public:
  OmniboxSuggestionRowButton(views::ButtonListener* listener,
                             const base::string16& text)
      : MdTextButton(listener, CONTEXT_OMNIBOX_PRIMARY) {
    SetText(text);
    set_ink_drop_highlight_opacity(CalculateInkDropHighlightOpacity());
  }

  OmniboxSuggestionRowButton(const OmniboxSuggestionRowButton&) = delete;
  OmniboxSuggestionRowButton& operator=(const OmniboxSuggestionRowButton&) =
      delete;

  ~OmniboxSuggestionRowButton() override = default;

  SkColor GetInkDropBaseColor() const override {
    return color_utils::GetColorWithMaxContrast(background()->get_color());
  }

  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override {
    // MdTextButton uses custom colors when creating ink drop highlight.
    // We need the base implementation that uses GetInkDropBaseColor for
    // highlight.
    return views::InkDropHostView::CreateInkDropHighlight();
  }

 private:
  float CalculateInkDropHighlightOpacity() {
    // Ink drop highlight opacity is result of mixing a layer with hovered
    // opacity and a layer with selected opacity. OmniboxPartState::SELECTED
    // opacity gets the same color as the selected omnibox row background (the
    // button would be the same color as the row) and overlaying it with
    // OmniboxPartState::HOVERED opacity makes the hovered button easily visible
    // in the selected or hovered row.
    return 1 - (1 - GetOmniboxStateOpacity(OmniboxPartState::HOVERED)) *
                   (1 - GetOmniboxStateOpacity(OmniboxPartState::SELECTED));
  }
};

namespace {

OmniboxSuggestionRowButton* CreatePillButton(
    OmniboxSuggestionButtonRowView* button_row,
    const char* message) {
  OmniboxSuggestionRowButton* button =
      button_row->AddChildView(std::make_unique<OmniboxSuggestionRowButton>(
          button_row, base::ASCIIToUTF16(message)));
  button->SetVisible(false);
  button->SetImageLabelSpacing(ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_RELATED_LABEL_HORIZONTAL_LIST));
  button->SetCustomPadding(
      ChromeLayoutProvider::Get()->GetInsetsMetric(INSETS_OMNIBOX_PILL_BUTTON));
  button->SetCornerRadius(button->GetInsets().height() +
                          GetLayoutConstant(LOCATION_BAR_ICON_SIZE));
  views::HighlightPathGenerator::Install(
      button, std::make_unique<views::PillHighlightPathGenerator>());
  return button;
}

}  // namespace

OmniboxSuggestionButtonRowView::OmniboxSuggestionButtonRowView(
    OmniboxPopupContentsView* popup_contents_view,
    int model_index)
    : popup_contents_view_(popup_contents_view), model_index_(model_index) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetCrossAxisAlignment(views::LayoutAlignment::kStart)
      .SetCollapseMargins(true)
      .SetInteriorMargin(
          gfx::Insets(0, OmniboxMatchCellView::GetTextIndent(),
                      ChromeLayoutProvider::Get()->GetDistanceMetric(
                          DISTANCE_OMNIBOX_CELL_VERTICAL_PADDING),
                      0))
      .SetDefault(
          views::kMarginsKey,
          gfx::Insets(0, ChromeLayoutProvider::Get()->GetDistanceMetric(
                             views::DISTANCE_RELATED_BUTTON_HORIZONTAL)));

  // TODO(orinj): Use the real translated string table values here instead.
  keyword_button_ = CreatePillButton(this, "Keyword search");
  pedal_button_ = CreatePillButton(this, "Pedal");
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

void OmniboxSuggestionButtonRowView::UpdateFromModel() {
  SetPillButtonVisibility(keyword_button_,
                          OmniboxPopupModel::FOCUSED_BUTTON_KEYWORD);
  if (keyword_button_->GetVisible()) {
    const OmniboxEditModel* edit_model = model()->edit_model();
    base::string16 keyword;
    bool is_keyword_hint = false;
    match().GetKeywordUIState(edit_model->client()->GetTemplateURLService(),
                              &keyword, &is_keyword_hint);

    const auto names = SelectedKeywordView::GetKeywordLabelNames(
        keyword, edit_model->client()->GetTemplateURLService());
    keyword_button_->SetText(names.full_name);
  }

  SetPillButtonVisibility(pedal_button_,
                          OmniboxPopupModel::FOCUSED_BUTTON_PEDAL);
  if (pedal_button_->GetVisible()) {
    pedal_button_->SetText(match().pedal->GetLabelStrings().hint);
    pedal_button_->SetTooltipText(
        match().pedal->GetLabelStrings().suggestion_contents);
  }

  SetPillButtonVisibility(tab_switch_button_,
                          OmniboxPopupModel::FOCUSED_BUTTON_TAB_SWITCH);

  bool is_any_button_visible = keyword_button_->GetVisible() ||
                               pedal_button_->GetVisible() ||
                               tab_switch_button_->GetVisible();
  SetVisible(is_any_button_visible);
}

void OmniboxSuggestionButtonRowView::OnStyleRefresh() {
  keyword_button_focus_ring_->SchedulePaint();
  pedal_button_focus_ring_->SchedulePaint();
  tab_switch_button_focus_ring_->SchedulePaint();
}

void OmniboxSuggestionButtonRowView::OnThemeChanged() {
  View::OnThemeChanged();
  SkColor color = GetOmniboxColor(GetThemeProvider(), OmniboxPart::RESULTS_ICON,
                                  OmniboxPartState::NORMAL);
  const int icon_size = GetLayoutConstant(LOCATION_BAR_ICON_SIZE);

  keyword_button_->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(vector_icons::kSearchIcon, icon_size, color));
  pedal_button_->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(omnibox::kProductIcon, icon_size, color));
  tab_switch_button_->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(omnibox::kSwitchIcon, icon_size, color));
}

void OmniboxSuggestionButtonRowView::ButtonPressed(views::Button* button,
                                                   const ui::Event& event) {
  OmniboxPopupModel* popup_model = popup_contents_view_->model();
  if (!popup_model)
    return;

  if (button == tab_switch_button_) {
    popup_model->TriggerSelectionAction(
        OmniboxPopupModel::Selection(
            model_index_, OmniboxPopupModel::FOCUSED_BUTTON_TAB_SWITCH),
        event.time_stamp());
  } else if (button == keyword_button_) {
    // TODO(yoangela): Port to PopupModel and merge with keyEvent
    // TODO(orinj): Clear out existing suggestions, particularly this one, as
    // once we AcceptKeyword, we are really in a new scope state and holding
    // onto old suggestions is confusing and error prone. Without this check,
    // a second click of the button violates assumptions in |AcceptKeyword|.
    if (model()->edit_model()->is_keyword_hint()) {
      auto method = metrics::OmniboxEventProto::INVALID;
      if (event.IsMouseEvent()) {
        method = metrics::OmniboxEventProto::CLICK_HINT_VIEW;
      } else if (event.IsGestureEvent()) {
        method = metrics::OmniboxEventProto::TAP_HINT_VIEW;
      }
      DCHECK_NE(method, metrics::OmniboxEventProto::INVALID);
      model()->edit_model()->AcceptKeyword(method);
    }
  } else if (button == pedal_button_) {
    popup_model->TriggerSelectionAction(
        OmniboxPopupModel::Selection(model_index_,
                                     OmniboxPopupModel::FOCUSED_BUTTON_PEDAL),
        event.time_stamp());
  }
}

const OmniboxPopupModel* OmniboxSuggestionButtonRowView::model() const {
  return popup_contents_view_->model();
}

const AutocompleteMatch& OmniboxSuggestionButtonRowView::match() const {
  return model()->result().match_at(model_index_);
}

void OmniboxSuggestionButtonRowView::SetPillButtonVisibility(
    views::MdTextButton* button,
    OmniboxPopupModel::LineState state) {
  button->SetVisible(model()->IsControlPresentOnMatch(
      OmniboxPopupModel::Selection(model_index_, state)));
}
