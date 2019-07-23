// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_list_view.h"

#include <algorithm>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/search/search_result.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/bind.h"
#include "base/time/time.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"

namespace app_list {

namespace {

constexpr int kMaxResults = 5;
constexpr base::TimeDelta kImpressionThreshold =
    base::TimeDelta::FromSeconds(3);

constexpr SkColor kListVerticalBarIconColor =
    SkColorSetARGB(0xFF, 0xE8, 0xEA, 0xED);

bool IsEmbeddedAssistantUiEnabled(AppListViewDelegate* view_delegate) {
  if (!app_list_features::IsEmbeddedAssistantUIEnabled())
    return false;

  return view_delegate && view_delegate->IsAssistantAllowedAndEnabled();
}

// Get the vector icon to update previous Assistant item.
const gfx::VectorIcon* GetPreviousVectorIcon(
    int continuous_assistant_item_count) {
  if (continuous_assistant_item_count == 2) {
    return &kVerticalBarSingleIcon;
  } else if (continuous_assistant_item_count > 2) {
    return &kVerticalBarEndIcon;
  }

  NOTREACHED();
  return nullptr;
}

// Get the vector icon to update current Assistant item.
const gfx::VectorIcon* GetCurrentVectorIcon(
    int continuous_assistant_item_count) {
  if (continuous_assistant_item_count == 1) {
    return &ash::kAssistantIcon;
  } else if (continuous_assistant_item_count == 2) {
    return &kVerticalBarStartIcon;
  } else if (continuous_assistant_item_count > 2) {
    return &kVerticalBarMiddleIcon;
  }

  NOTREACHED();
  return nullptr;
}

// Calculate the display icons for Assistant items.
// We have the following situations:
// Number of consecutive Assistant items:
// 1 item       -> Assistant icon.
// 2 items      -> Assistant icon + single vertical bar icon.
// 3 items      -> Assistant icon + start + end vertical bar icons.
// n >= 4 items -> Assistant icon + start + middle (n - 3) + end vertical bar
//                 icons.
// This algo sets current result's vertical icon based on the
// |continuous_assistant_item_count|, but also needs to update previous result's
// vertical icon if current result is not an Assisttant item or previous result
// is the last result.
void CalculateDisplayIcons(
    const std::vector<SearchResult*>& display_results,
    std::vector<const gfx::VectorIcon*>* out_display_icons) {
  const size_t display_size = display_results.size();
  int continuous_assistant_item_count = 0;
  // Index |i| goes beyond the last display result to update its icon.
  for (size_t i = 0; i <= display_size; ++i) {
    if (i < display_size && display_results[i]->is_omnibox_search()) {
      ++continuous_assistant_item_count;
    } else {
      // Update previous result's icon.
      if (continuous_assistant_item_count >= 2) {
        (*out_display_icons)[i - 1] =
            GetPreviousVectorIcon(continuous_assistant_item_count);
      }

      continuous_assistant_item_count = 0;
    }

    // Update current result's icon.
    if (continuous_assistant_item_count > 0) {
      (*out_display_icons)[i] =
          GetCurrentVectorIcon(continuous_assistant_item_count);
    }
  }
}

ash::SearchResultIdWithPositionIndices GetSearchResultsForLogging(
    std::vector<SearchResultView*> search_result_views) {
  ash::SearchResultIdWithPositionIndices results;
  for (const auto* item : search_result_views) {
    if (item->result()) {
      results.emplace_back(ash::SearchResultIdWithPositionIndex(
          item->result()->id(), item->index_in_container()));
    }
  }
  return results;
}
}  // namespace

SearchResultListView::SearchResultListView(AppListMainView* main_view,
                                           AppListViewDelegate* view_delegate)
    : SearchResultContainerView(view_delegate),
      main_view_(main_view),
      view_delegate_(view_delegate),
      results_container_(new views::View) {
  results_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  for (int i = 0; i < kMaxResults; ++i) {
    search_result_views_.emplace_back(
        new SearchResultView(this, view_delegate_));
    search_result_views_.back()->set_index_in_container(i);
    results_container_->AddChildView(search_result_views_.back());
    AddObservedResultView(search_result_views_.back());
  }
  AddChildView(results_container_);
}

SearchResultListView::~SearchResultListView() = default;

void SearchResultListView::ListItemsRemoved(size_t start, size_t count) {
  size_t last = std::min(start + count, search_result_views_.size());
  for (size_t i = start; i < last; ++i)
    GetResultViewAt(i)->ClearResult();

  SearchResultContainerView::ListItemsRemoved(start, count);
}

SearchResultView* SearchResultListView::GetResultViewAt(size_t index) {
  DCHECK(index >= 0 && index < search_result_views_.size());
  return search_result_views_[index];
}

void SearchResultListView::NotifyFirstResultYIndex(int y_index) {
  for (size_t i = 0; i < static_cast<size_t>(num_results()); ++i)
    GetResultViewAt(i)->result()->set_distance_from_origin(i + y_index);
}

int SearchResultListView::GetYSize() {
  return num_results();
}

SearchResultBaseView* SearchResultListView::GetFirstResultView() {
  DCHECK(!results_container_->children().empty());
  return num_results() <= 0 ? nullptr : search_result_views_[0];
}

int SearchResultListView::DoUpdate() {
  std::vector<SearchResult*> display_results =
      SearchModel::FilterSearchResultsByDisplayType(
          results(), ash::SearchResultDisplayType::kList, /*excludes=*/{},
          results_container_->children().size());

  const size_t display_size = display_results.size();
  std::vector<const gfx::VectorIcon*> assistant_item_icons(display_size,
                                                           nullptr);
  if (IsEmbeddedAssistantUiEnabled(view_delegate_))
    CalculateDisplayIcons(display_results, &assistant_item_icons);

  for (size_t i = 0; i < results_container_->children().size(); ++i) {
    SearchResultView* result_view = GetResultViewAt(i);
    if (i < display_results.size()) {
      if (assistant_item_icons[i]) {
        result_view->SetDisplayIcon(gfx::CreateVectorIcon(
            *(assistant_item_icons[i]),
            (assistant_item_icons[i] == &ash::kAssistantIcon)
                ? AppListConfig::instance().search_list_icon_dimension()
                : AppListConfig::instance()
                      .search_list_icon_vertical_bar_dimension(),
            kListVerticalBarIconColor));
      } else {
        // Reset |display_icon_|.
        result_view->SetDisplayIcon(gfx::ImageSkia());
      }
      result_view->SetResult(display_results[i]);
      result_view->SetVisible(true);
    } else {
      result_view->SetResult(NULL);
      result_view->SetVisible(false);
    }
  }

  // Logic for logging impression of items that were shown to user.
  // Each time DoUpdate() called, start a timer that will be fired after a
  // certain amount of time |kImpressionThreshold|. If during the waiting time,
  // there's another DoUpdate() called, reset the timer and start a new timer
  // with updated result list.
  if (impression_timer_.IsRunning())
    impression_timer_.Stop();
  impression_timer_.Start(FROM_HERE, kImpressionThreshold, this,
                          &SearchResultListView::LogImpressions);

  set_container_score(
      display_results.empty() ? 0 : display_results.front()->display_score());

  return display_results.size();
}

void SearchResultListView::LogImpressions() {
  // Since no items is actually clicked, send the position index of clicked item
  // as -1.
  if (main_view_->search_box_view()->is_search_box_active()) {
    view_delegate_->NotifySearchResultsForLogging(
        view_delegate_->GetSearchModel()->search_box()->text(),
        GetSearchResultsForLogging(search_result_views_),
        -1 /* position_index */);
  }
}

void SearchResultListView::Layout() {
  results_container_->SetBoundsRect(GetLocalBounds());
}

gfx::Size SearchResultListView::CalculatePreferredSize() const {
  return results_container_->GetPreferredSize();
}

const char* SearchResultListView::GetClassName() const {
  return "SearchResultListView";
}

int SearchResultListView::GetHeightForWidth(int w) const {
  return results_container_->GetHeightForWidth(w);
}

void SearchResultListView::SearchResultActivated(SearchResultView* view,
                                                 int event_flags) {
  if (view_delegate_ && view->result()) {
    RecordSearchResultOpenSource(view->result(), view_delegate_->GetModel(),
                                 view_delegate_->GetSearchModel());
    view_delegate_->LogResultLaunchHistogram(
        SearchResultLaunchLocation::kResultList, view->index_in_container());
    view_delegate_->NotifySearchResultsForLogging(
        view_delegate_->GetSearchModel()->search_box()->text(),
        GetSearchResultsForLogging(search_result_views_),
        view->index_in_container());
    view_delegate_->OpenSearchResult(
        view->result()->id(), event_flags,
        ash::AppListLaunchedFrom::kLaunchedFromSearchBox,
        ash::AppListLaunchType::kSearchResult, -1 /* suggestion_index */);
  }
}

void SearchResultListView::SearchResultActionActivated(SearchResultView* view,
                                                       size_t action_index,
                                                       int event_flags) {
  if (view_delegate_ && view->result()) {
    ash::OmniBoxZeroStateAction action =
        ash::GetOmniBoxZeroStateAction(action_index);
    if (action == ash::OmniBoxZeroStateAction::kRemoveSuggestion) {
      view_delegate_->InvokeSearchResultAction(view->result()->id(),
                                               action_index, event_flags);
    } else if (action == ash::OmniBoxZeroStateAction::kAppendSuggestion) {
      // Make sure ChromeVox will focus on the search box.
      main_view_->search_box_view()->search_box()->NotifyAccessibilityEvent(
          ax::mojom::Event::kSelection, true);
      main_view_->search_box_view()->UpdateQuery(view->result()->title());
    }
  }
}

void SearchResultListView::OnSearchResultInstalled(SearchResultView* view) {
  if (main_view_ && view->result())
    main_view_->OnResultInstalled(view->result());
}

bool SearchResultListView::HandleVerticalFocusMovement(SearchResultView* view,
                                                       bool arrow_up) {
  int view_index = -1;
  for (int i = 0; i < num_results(); ++i) {
    if (view == GetResultViewAt(i)) {
      view_index = i;
      break;
    }
  }

  if (view_index == -1) {
    // Not found in the result list.
    NOTREACHED();
    return false;
  }

  if (arrow_up) {  // VKEY_UP
    if (view_index > 0) {
      // Move to the previous result if the current one is not the first result.
      GetResultViewAt(view_index - 1)->RequestFocus();
      return true;
    }
  } else {  // VKEY_DOWN
    // Move down to the next result if the currernt one is not the last result;
    // otherwise, move focus to search box.
    if (view_index == num_results() - 1)
      main_view_->search_box_view()->search_box()->RequestFocus();
    else
      GetResultViewAt(view_index + 1)->RequestFocus();
    return true;
  }

  return false;
}

}  // namespace app_list
