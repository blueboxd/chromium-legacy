// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_LIST_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_LIST_VIEW_H_

#include <stddef.h>
#include <vector>

#include "ash/app_list/views/search_result_container_view.h"
#include "ash/app_list/views/search_result_view.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/views/view.h"

namespace app_list {
namespace test {
class SearchResultListViewTest;
}

class AppListMainView;
class AppListViewDelegate;

// SearchResultListView displays SearchResultList with a list of
// SearchResultView.
class APP_LIST_EXPORT SearchResultListView : public SearchResultContainerView {
 public:
  SearchResultListView(AppListMainView* main_view,
                       AppListViewDelegate* view_delegate);
  ~SearchResultListView() override;

  void SearchResultActivated(SearchResultView* view, int event_flags);

  void SearchResultActionActivated(SearchResultView* view,
                                   size_t action_index,
                                   int event_flags);

  void OnSearchResultInstalled(SearchResultView* view);

  // Handles vertical focus movement triggered by VKEY_UP/VKEY_DOWN.
  bool HandleVerticalFocusMovement(SearchResultView* view, bool arrow_up);

  // Overridden from views::View:
  gfx::Size CalculatePreferredSize() const override;
  const char* GetClassName() const override;

  // Overridden from ui::ListModelObserver:
  void ListItemsRemoved(size_t start, size_t count) override;

  // Overridden from SearchResultContainerView:
  SearchResultView* GetResultViewAt(size_t index) override;
  void NotifyFirstResultYIndex(int y_index) override;
  int GetYSize() override;
  SearchResultBaseView* GetFirstResultView() override;

  AppListMainView* app_list_main_view() const { return main_view_; }

 private:
  friend class test::SearchResultListViewTest;

  // Overridden from SearchResultContainerView:
  int DoUpdate() override;

  // Overridden from views::View:
  void Layout() override;
  int GetHeightForWidth(int w) const override;
  // Log the set of recommendations (impression) that were shown to the user
  // after a priod of time.
  void LogImpressions();

  AppListMainView* main_view_;          // Owned by views hierarchy.
  AppListViewDelegate* view_delegate_;  // Not owned.

  views::View* results_container_;

  std::vector<SearchResultView*> search_result_views_;  // Not owned.

  // Used for logging impression shown to users.
  base::OneShotTimer impression_timer_;

  DISALLOW_COPY_AND_ASSIGN(SearchResultListView);
};

}  // namespace app_list

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_LIST_VIEW_H_
