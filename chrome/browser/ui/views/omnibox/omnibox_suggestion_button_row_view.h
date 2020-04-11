// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_SUGGESTION_BUTTON_ROW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_SUGGESTION_BUTTON_ROW_VIEW_H_

#include "base/macros.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/view.h"

class OmniboxPopupContentsView;

// A view to contain the button row within a result view.
class OmniboxSuggestionButtonRowView : public views::View,
                                       public views::ButtonListener {
 public:
  explicit OmniboxSuggestionButtonRowView(OmniboxPopupContentsView* view,
                                          int model_index);
  ~OmniboxSuggestionButtonRowView() override;

  // Gets keyword information and applies it to the keyword button label.
  // TODO(orinj): This should eventually be made private after refactoring.
  void UpdateKeyword();

  // Called when themes, styles, and visibility is refreshed in result view.
  void OnStyleRefresh();

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::View:
  void Layout() override;

 private:
  // Get the popup model from the view.
  const OmniboxPopupModel* model() const;

  // Digs into the model with index to get the match for owning result view.
  const AutocompleteMatch& match() const;

  OmniboxPopupContentsView* const popup_contents_view_;
  size_t const model_index_;

  views::MdTextButton* keyword_button_ = nullptr;
  views::MdTextButton* pedal_button_ = nullptr;
  views::MdTextButton* tab_switch_button_ = nullptr;
  std::unique_ptr<views::FocusRing> keyword_button_focus_ring_;
  std::unique_ptr<views::FocusRing> pedal_button_focus_ring_;
  std::unique_ptr<views::FocusRing> tab_switch_button_focus_ring_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxSuggestionButtonRowView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_SUGGESTION_BUTTON_ROW_VIEW_H_
