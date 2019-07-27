// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_ACTIONS_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_ACTIONS_VIEW_H_

#include <vector>

#include "ash/app_list/model/search/search_result.h"
#include "base/macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace app_list {

class SearchResultActionsViewDelegate;
class SearchResultView;
class SearchResultImageButton;

// SearchResultActionsView displays a SearchResult::Actions in a button
// strip. Each action is presented as a button and horizontally laid out.
class SearchResultActionsView : public views::View,
                                public views::ButtonListener {
 public:
  explicit SearchResultActionsView(SearchResultActionsViewDelegate* delegate);
  ~SearchResultActionsView() override;

  void SetActions(const SearchResult::Actions& actions);

  bool IsValidActionIndex(size_t action_index) const;

  bool IsSearchResultHoveredOrSelected() const;

  // Updates the button UI upon the SearchResultView's UI state change.
  void UpdateButtonsOnStateChanged();

  // Called when one of the action buttons changes state.
  void ActionButtonStateChanged();

  // views::View:
  const char* GetClassName() const override;

 private:
  void CreateImageButton(const SearchResult::Action& action);

  // views::View overrides:
  void ChildVisibilityChanged(views::View* child) override;

  // views::ButtonListener overrides:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  SearchResultActionsViewDelegate* delegate_;  // Not owned.
  std::vector<SearchResultImageButton*> buttons_;

  DISALLOW_COPY_AND_ASSIGN(SearchResultActionsView);
};

}  // namespace app_list

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_ACTIONS_VIEW_H_
