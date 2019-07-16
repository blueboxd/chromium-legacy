// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/suggestion_chip_container_view.h"

#include <algorithm>
#include <memory>

#include "ash/app_list/app_list_util.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "base/bind.h"
#include "base/callback.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"

namespace app_list {

namespace {

// The spacing between chips.
constexpr int kChipSpacing = 8;

bool IsPolicySuggestionChip(const SearchResult& result) {
  return result.display_location() ==
             ash::SearchResultDisplayLocation::kSuggestionChipContainer &&
         result.display_index() != ash::SearchResultDisplayIndex::kUndefined;
}

}  // namespace

SuggestionChipContainerView::SuggestionChipContainerView(
    ContentsView* contents_view)
    : SearchResultContainerView(
          contents_view != nullptr
              ? contents_view->GetAppListMainView()->view_delegate()
              : nullptr),
      contents_view_(contents_view) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  DCHECK(contents_view);
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          kChipSpacing));
  layout_manager->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);

  for (size_t i = 0; i < static_cast<size_t>(
                             AppListConfig::instance().num_start_page_tiles());
       ++i) {
    SearchResultSuggestionChipView* chip =
        new SearchResultSuggestionChipView(view_delegate());
    chip->SetVisible(false);
    chip->set_index_in_container(i);
    suggestion_chip_views_.emplace_back(chip);
    AddChildView(chip);
  }
}

SuggestionChipContainerView::~SuggestionChipContainerView() = default;

SearchResultSuggestionChipView* SuggestionChipContainerView::GetResultViewAt(
    size_t index) {
  DCHECK(index >= 0 && index < suggestion_chip_views_.size());
  return suggestion_chip_views_[index];
}

int SuggestionChipContainerView::DoUpdate() {
  if (IgnoreUpdateAndLayout())
    return num_results();

  // Filter out priority suggestion chips with a non-default value
  // for |display_index|.
  auto filter_indexed_policy_chips = [](const SearchResult& r) -> bool {
    return IsPolicySuggestionChip(r);
  };
  std::vector<SearchResult*> indexed_policy_results =
      SearchModel::FilterSearchResultsByFunction(
          results(), base::BindRepeating(filter_indexed_policy_chips),
          AppListConfig::instance().num_start_page_tiles());

  std::sort(indexed_policy_results.begin(), indexed_policy_results.end(),
            [](const SearchResult* r1, const SearchResult* r2) -> bool {
              return r1->display_index() < r2->display_index();
            });

  // Need to filter out kArcAppShortcut since it will be confusing to users
  // if shortcuts are displayed as suggestion chips. Also filter out any
  // duplicate policy chip results.
  auto filter_reinstall_and_shortcut = [](const SearchResult& r) -> bool {
    return r.display_type() == ash::SearchResultDisplayType::kRecommendation &&
           r.result_type() != ash::SearchResultType::kPlayStoreReinstallApp &&
           r.result_type() != ash::SearchResultType::kArcAppShortcut &&
           !IsPolicySuggestionChip(r);
  };
  std::vector<SearchResult*> display_results =
      SearchModel::FilterSearchResultsByFunction(
          results(), base::BindRepeating(filter_reinstall_and_shortcut),
          AppListConfig::instance().num_start_page_tiles() -
              indexed_policy_results.size());

  // Update display results list by placing policy result chips at their
  // specified |display_index|.
  for (auto* result : indexed_policy_results) {
    display_results.emplace(display_results.begin() + result->display_index(),
                            result);
  }
  // Update search results here, but wait until layout to add them as child
  // views when we know this view's bounds.
  for (size_t i = 0; i < static_cast<size_t>(
                             AppListConfig::instance().num_start_page_tiles());
       ++i) {
    suggestion_chip_views_[i]->SetResult(
        i < display_results.size() ? display_results[i] : nullptr);
  }

  Layout();
  return std::min(AppListConfig::instance().num_start_page_tiles(),
                  display_results.size());
}

const char* SuggestionChipContainerView::GetClassName() const {
  return "SuggestionChipContainerView";
}

void SuggestionChipContainerView::Layout() {
  if (IgnoreUpdateAndLayout())
    return;

  // Only show the chips that fit in this view's contents bounds.
  int total_width = 0;
  const int max_width = GetContentsBounds().width();
  for (auto* chip : suggestion_chip_views_) {
    if (!chip->result())
      break;
    const gfx::Size size = chip->GetPreferredSize();
    if (size.width() + total_width > max_width) {
      chip->SetVisible(false);
    } else {
      chip->SetVisible(true);
      chip->SetSize(size);
    }
    total_width += (total_width == 0 ? 0 : kChipSpacing) + size.width();
  }

  views::View::Layout();
}

bool SuggestionChipContainerView::OnKeyPressed(const ui::KeyEvent& event) {
  // Let the FocusManager handle Left/Right keys.
  if (!IsUnhandledUpDownKeyEvent(event))
    return false;

  // Up key moves focus to the search box. Down key moves focus to the first
  // app.
  views::View* v = nullptr;
  if (event.key_code() == ui::VKEY_UP) {
    v = contents_view_->GetSearchBoxView()->search_box();
  } else {
    // The first app is the next to this view's last focusable view.
    views::View* last_focusable_view =
        GetFocusManager()->GetNextFocusableView(this, nullptr, true, false);
    v = GetFocusManager()->GetNextFocusableView(last_focusable_view, nullptr,
                                                false, false);
  }
  if (v)
    v->RequestFocus();
  return true;
}

void SuggestionChipContainerView::DisableFocusForShowingActiveFolder(
    bool disabled) {
  for (auto* chip : suggestion_chip_views_)
    chip->SetEnabled(!disabled);

  // Ignore the container view in accessibility tree so that suggestion chips
  // will not be accessed by ChromeVox.
  GetViewAccessibility().OverrideIsIgnored(disabled);
  GetViewAccessibility().NotifyAccessibilityEvent(
      ax::mojom::Event::kTreeChanged);
}

void SuggestionChipContainerView::OnTabletModeChanged(bool started) {
  // Enable/Disable chips' background blur based on tablet mode.
  for (auto* chip : suggestion_chip_views_)
    chip->SetBackgroundBlurEnabled(started);
}

bool SuggestionChipContainerView::IgnoreUpdateAndLayout() const {
  // Ignore update and layout when this view is not shown.
  const ash::AppListState state = contents_view_->GetActiveState();
  return state != ash::AppListState::kStateApps;
}

}  // namespace app_list
