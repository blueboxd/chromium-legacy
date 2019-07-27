// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_BASE_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_BASE_VIEW_H_

#include "ash/app_list/app_list_export.h"
#include "ash/app_list/model/search/search_result_observer.h"
#include "base/optional.h"
#include "ui/views/controls/button/button.h"

namespace app_list {

class SearchResult;

// Base class for views that observe and display a search result
class APP_LIST_EXPORT SearchResultBaseView : public views::Button,
                                             public views::ButtonListener,
                                             public SearchResultObserver {
 public:
  SearchResultBaseView();

  // Set or remove the background highlight.
  void SetBackgroundHighlighted(bool enabled);

  SearchResult* result() const { return result_; }
  void SetResult(SearchResult* result);

  // Invoked before changing |result_| to |new_result|.
  virtual void OnResultChanging(SearchResult* new_result) {}

  // Invoked after |result_| is updated.
  virtual void OnResultChanged() {}

  // Overridden from SearchResultObserver:
  void OnResultDestroying() override;

  // Computes the button's spoken feedback name.
  virtual base::string16 ComputeAccessibleName() const;

  // Clears the result without calling |OnResultChanged| or |OnResultChanging|
  void ClearResult();

  bool background_highlighted() const { return background_highlighted_; }

  int index_in_container() const { return index_in_container_.value(); }

  void set_index_in_container(size_t index) { index_in_container_ = index; }

  // views::Button:
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override;

  // views::View:
  const char* GetClassName() const override;

 protected:
  ~SearchResultBaseView() override;

  void UpdateAccessibleName();

 private:
  bool background_highlighted_ = false;

  // The index of this view within a |SearchResultContainerView| that holds it.
  base::Optional<int> index_in_container_;

  SearchResult* result_ = nullptr;  // Owned by SearchModel::SearchResults.

  DISALLOW_COPY_AND_ASSIGN(SearchResultBaseView);
};

}  // namespace app_list

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_BASE_VIEW_H_
