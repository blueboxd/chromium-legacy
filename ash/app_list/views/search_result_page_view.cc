// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_page_view.h"

#include <stddef.h>

#include <algorithm>
#include <memory>

#include "ash/app_list/app_list_util.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/assistant/privacy_info_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_base_view.h"
#include "ash/app_list/views/search_result_list_view.h"
#include "ash/app_list/views/search_result_tile_item_list_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/view_shadow.h"
#include "base/memory/ptr_util.h"
#include "ui/chromeos/search_box/search_box_constants.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/scrollbar/overlay_scroll_bar.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace app_list {

namespace {

constexpr int kHeight = 440;
constexpr int kWidth = 640;

// The horizontal padding of the separator.
constexpr int kSeparatorPadding = 12;
constexpr int kSeparatorThickness = 1;

// The height of the search box in this page.
constexpr int kSearchBoxHeight = 56;

// The spacing between search box bottom and separator line.
// Add 1 pixel spacing so that the search bbox bottom will not paint over
// the separator line drawn by SearchResultPageBackground in some scale factors
// due to the round up.
constexpr int kSearchBoxBottomSpacing = 1;

constexpr SkColor kSeparatorColor = SkColorSetA(gfx::kGoogleGrey900, 0x24);

// The shadow elevation value for the shadow of the expanded search box.
constexpr int kSearchBoxSearchResultShadowElevation = 12;

// A container view that ensures the card background and the shadow are painted
// in the correct order.
class SearchCardView : public views::View {
 public:
  explicit SearchCardView(views::View* content_view) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    AddChildView(content_view);
  }

  // views::View overrides:
  const char* GetClassName() const override { return "SearchCardView"; }

  ~SearchCardView() override {}
};

class ZeroWidthVerticalScrollBar : public views::OverlayScrollBar {
 public:
  ZeroWidthVerticalScrollBar() : OverlayScrollBar(false) {}

  // OverlayScrollBar overrides:
  int GetThickness() const override { return 0; }

  bool OnKeyPressed(const ui::KeyEvent& event) override {
    // Arrow keys should be handled by FocusManager to move focus. When a search
    // result is focused, it will be set visible in scroll view.
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ZeroWidthVerticalScrollBar);
};

class SearchResultPageBackground : public views::Background {
 public:
  explicit SearchResultPageBackground(SkColor color) {
    SetNativeControlColor(color);
  }
  ~SearchResultPageBackground() override = default;

 private:
  // views::Background overrides:
  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    canvas->DrawColor(get_color());
    gfx::Rect bounds = view->GetContentsBounds();
    if (bounds.height() <= kSearchBoxHeight)
      return;
    // Draw a separator between SearchBoxView and SearchResultPageView.
    bounds.set_y(kSearchBoxHeight + kSearchBoxBottomSpacing);
    bounds.set_height(kSeparatorThickness);
    canvas->FillRect(bounds, kSeparatorColor);
  }

  DISALLOW_COPY_AND_ASSIGN(SearchResultPageBackground);
};

}  // namespace

class SearchResultPageView::HorizontalSeparator : public views::View {
 public:
  explicit HorizontalSeparator(int preferred_width)
      : preferred_width_(preferred_width) {
    SetBorder(views::CreateEmptyBorder(
        gfx::Insets(0, kSeparatorPadding, 0, kSeparatorPadding)));
  }

  ~HorizontalSeparator() override {}

  // views::View overrides:
  const char* GetClassName() const override { return "HorizontalSeparator"; }

  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(preferred_width_, kSeparatorThickness);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    gfx::Rect rect = GetContentsBounds();
    canvas->FillRect(rect, kSeparatorColor);
    View::OnPaint(canvas);
  }

 private:
  const int preferred_width_;

  DISALLOW_COPY_AND_ASSIGN(HorizontalSeparator);
};

SearchResultPageView::SearchResultPageView(AppListViewDelegate* view_delegate)
    : view_delegate_(view_delegate), contents_view_(new views::View) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  contents_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(), 0));

  if (view_delegate_->ShouldShowAssistantPrivacyInfo()) {
    assistant_privacy_info_view_ = new PrivacyInfoView(view_delegate_, this);
    contents_view_->AddChildView(assistant_privacy_info_view_);
  }

  view_shadow_ = std::make_unique<ash::ViewShadow>(
      this, kSearchBoxSearchResultShadowElevation);
  view_shadow_->SetRoundedCornerRadius(
      search_box::kSearchBoxBorderCornerRadiusSearchResult);

  // Hides this view behind the search box by using the same color and
  // background border corner radius. All child views' background should be
  // set transparent so that the rounded corner is not overwritten.
  SetBackground(std::make_unique<SearchResultPageBackground>(
      AppListConfig::instance().card_background_color()));
  views::ScrollView* const scroller = new views::ScrollView;
  // Leaves a placeholder area for the search box and the separator below it.
  scroller->SetBorder(views::CreateEmptyBorder(gfx::Insets(
      kSearchBoxHeight + kSearchBoxBottomSpacing + kSeparatorThickness, 0, 0,
      0)));
  scroller->SetDrawOverflowIndicator(false);
  scroller->SetContents(base::WrapUnique(contents_view_));
  // Setting clip height is necessary to make ScrollView take into account its
  // contents' size. Using zeroes doesn't prevent it from scrolling and sizing
  // correctly.
  scroller->ClipHeightTo(0, 0);
  scroller->SetVerticalScrollBar(new ZeroWidthVerticalScrollBar);
  scroller->SetBackgroundColor(SK_ColorTRANSPARENT);
  AddChildView(scroller);

  SetLayoutManager(std::make_unique<views::FillLayout>());

  result_selection_controller_ =
      std::make_unique<ResultSelectionController>(&result_container_views_);
}

SearchResultPageView::~SearchResultPageView() = default;

void SearchResultPageView::AddSearchResultContainerView(
    SearchModel::SearchResults* results_model,
    SearchResultContainerView* result_container) {
  if (!result_container_views_.empty()) {
    HorizontalSeparator* separator = new HorizontalSeparator(bounds().width());
    contents_view_->AddChildView(separator);
    separators_.push_back(separator);
  }
  contents_view_->AddChildView(new SearchCardView(result_container));
  result_container_views_.push_back(result_container);
  result_container->SetResults(results_model);
  result_container->set_delegate(this);
}

bool SearchResultPageView::IsFirstResultTile() const {
  // In the event that the result does not exist, it is not a tile.
  if (!first_result_view_ || !first_result_view_->result())
    return false;

  // |kRecommendation| result type refers to tiles in Zero State.
  return first_result_view_->result()->display_type() ==
             ash::SearchResultDisplayType::kTile ||
         first_result_view_->result()->display_type() ==
             ash::SearchResultDisplayType::kRecommendation;
}

bool SearchResultPageView::IsFirstResultHighlighted() const {
  DCHECK(first_result_view_);
  return first_result_view_->selected();
}

bool SearchResultPageView::OnKeyPressed(const ui::KeyEvent& event) {
  // Let the FocusManager handle Left/Right keys.
  if (!IsUnhandledUpDownKeyEvent(event))
    return false;

  views::View* next_focusable_view = nullptr;
  if (event.key_code() == ui::VKEY_UP) {
    next_focusable_view = GetFocusManager()->GetNextFocusableView(
        GetFocusManager()->GetFocusedView(), GetWidget(), true, false);
  } else {
    DCHECK_EQ(event.key_code(), ui::VKEY_DOWN);
    next_focusable_view = GetFocusManager()->GetNextFocusableView(
        GetFocusManager()->GetFocusedView(), GetWidget(), false, false);
  }

  if (next_focusable_view && !Contains(next_focusable_view)) {
    // Hitting up key when focus is on first search result or hitting down
    // key when focus is on last search result should move focus onto search
    // box and select all text.
    views::Textfield* search_box =
        AppListPage::contents_view()->GetSearchBoxView()->search_box();
    search_box->RequestFocus();
    search_box->SelectAll(false);
    return true;
  }

  // Return false to let FocusManager to handle default focus move by key
  // events.
  return false;
}

const char* SearchResultPageView::GetClassName() const {
  return "SearchResultPageView";
}

gfx::Size SearchResultPageView::CalculatePreferredSize() const {
  return gfx::Size(kWidth, kHeight);
}

void SearchResultPageView::ReorderSearchResultContainers() {
  int view_offset = 0;
  if (assistant_privacy_info_view_) {
    const bool show_privacy_info =
        view_delegate_->ShouldShowAssistantPrivacyInfo();
    view_offset = show_privacy_info ? 1 : 0;
    assistant_privacy_info_view_->SetVisible(show_privacy_info);
  }

  // Sort the result container views by their score.
  std::sort(result_container_views_.begin(), result_container_views_.end(),
            [](const SearchResultContainerView* a,
               const SearchResultContainerView* b) -> bool {
              return a->container_score() > b->container_score();
            });

  int result_y_index = 0;
  for (size_t i = 0; i < result_container_views_.size(); ++i) {
    SearchResultContainerView* view = result_container_views_[i];

    if (i > 0) {
      HorizontalSeparator* separator = separators_[i - 1];
      // Hides the separator above the container that has no results.
      if (!view->container_score())
        separator->SetVisible(false);
      else
        separator->SetVisible(true);

      contents_view_->ReorderChildView(separator, i * 2 - 1 + view_offset);
      contents_view_->ReorderChildView(view->parent(), i * 2 + view_offset);

      result_y_index += kSeparatorThickness;
    } else {
      contents_view_->ReorderChildView(view->parent(), i + view_offset);
    }

    view->NotifyFirstResultYIndex(result_y_index);

    result_y_index += view->GetYSize();
  }

  Layout();
}

void SearchResultPageView::OnSearchResultContainerResultsChanging() {
  // Block any result selection changes while result updates are in flight.
  // The selection will be reset once the results are all updated.
  if (app_list_features::IsSearchBoxSelectionEnabled())
    result_selection_controller_->set_block_selection_changes(true);
}

void SearchResultPageView::OnSearchResultContainerResultsChanged() {
  DCHECK(!result_container_views_.empty());
  DCHECK(result_container_views_.size() == separators_.size() + 1);

  // Only sort and layout the containers when they have all updated.
  for (SearchResultContainerView* view : result_container_views_) {
    if (view->UpdateScheduled())
      return;
  }

  ReorderSearchResultContainers();

  if (!app_list_features::IsSearchBoxSelectionEnabled()) {
    views::View* focused_view = GetFocusManager()->GetFocusedView();

    // Clear the first search result view's background highlight.
    if (first_result_view_ && first_result_view_ != focused_view)
      first_result_view_->SetSelected(false, base::nullopt);
  }

  first_result_view_ = result_container_views_[0]->GetFirstResultView();
  if (!first_result_view_)
    return;

  if (!app_list_features::IsSearchBoxSelectionEnabled()) {
    views::View* focused_view = GetFocusManager()->GetFocusedView();
    // If one of the search result is focused, do not highlight the first search
    // result.
    if (Contains(focused_view))
      return;
  }

  // Update SearchBoxView search box autocomplete as necessary based on new
  // first result view.
  AppListPage::contents_view()->GetSearchBoxView()->ProcessAutocomplete();

  if (app_list_features::IsSearchBoxSelectionEnabled()) {
    // Reset selection to first when things change.
    result_selection_controller_->set_block_selection_changes(false);
    result_selection_controller_->ResetSelection(nullptr /*key_event*/);
  } else {
    // Highlight the first result after search results are updated. Note that
    // the focus is not set on the first result to prevent frequent focus switch
    // between the search box and the first result when the user is typing
    // query.
    first_result_view_->SetSelected(true, base::nullopt);
  }
}

void SearchResultPageView::OnSearchResultContainerResultFocused(
    SearchResultBaseView* focused_result_view) {
  if (!focused_result_view->result())
    return;

  if (app_list_features::IsSearchBoxSelectionEnabled())
    return;

  views::Textfield* search_box =
      AppListPage::contents_view()->GetSearchBoxView()->search_box();
  if (focused_result_view->result()->result_type() ==
          ash::SearchResultType::kOmnibox &&
      !focused_result_view->result()->is_omnibox_search()) {
    search_box->SetText(focused_result_view->result()->details());
  } else {
    search_box->SetText(focused_result_view->result()->title());
  }
}

void SearchResultPageView::OnAssistantPrivacyInfoViewCloseButtonPressed() {
  ReorderSearchResultContainers();
}

void SearchResultPageView::OnHidden() {
  // Hide the search results page when it is behind search box to avoid focus
  // being moved onto suggested apps when zero state is enabled.
  AppListPage::OnHidden();
  SetVisible(false);
  for (auto* container_view : result_container_views_) {
    container_view->SetShown(false);
  }
}

void SearchResultPageView::OnShown() {
  AppListPage::OnShown();
  for (auto* container_view : result_container_views_) {
    container_view->SetShown(true);
  }
}

void SearchResultPageView::OnAnimationStarted(ash::AppListState from_state,
                                              ash::AppListState to_state) {
  if (from_state != ash::AppListState::kStateSearchResults &&
      to_state != ash::AppListState::kStateSearchResults) {
    return;
  }

  const ContentsView* const contents_view = AppListPage::contents_view();
  const gfx::Rect contents_bounds = contents_view->GetContentsBounds();
  const gfx::Rect from_rect =
      GetPageBoundsForState(from_state, contents_bounds,
                            contents_view->GetSearchBoxBounds(from_state));
  const gfx::Rect to_rect = GetPageBoundsForState(
      to_state, contents_bounds, contents_view->GetSearchBoxBounds(to_state));
  if (from_rect == to_rect)
    return;

  const int to_radius =
      contents_view->GetSearchBoxView()->GetSearchBoxBorderCornerRadiusForState(
          to_state);

  // Here does the following animations;
  // - clip-rect, so it looks like expanding from |from_rect| to |to_rect|.
  // - rounded-rect
  // - transform of the shadow
  SetBoundsRect(to_rect);
  gfx::Rect clip_rect = from_rect;
  clip_rect -= to_rect.OffsetFromOrigin();
  layer()->SetClipRect(clip_rect);
  {
    auto settings = contents_view->CreateTransitionAnimationSettings(layer());
    layer()->SetClipRect(gfx::Rect(to_rect.size()));
    // This changes the shadow's corner immediately while this corner bounds
    // gradually. This would be fine because this would be unnoticeable to
    // users.
    view_shadow_->SetRoundedCornerRadius(to_radius);
  }

  // Animate the shadow's bounds through transform.
  {
    gfx::Transform transform;
    transform.Translate(from_rect.origin() - to_rect.origin());
    transform.Scale(static_cast<float>(from_rect.width()) / to_rect.width(),
                    static_cast<float>(from_rect.height()) / to_rect.height());
    view_shadow_->shadow()->layer()->SetTransform(transform);

    auto settings = contents_view->CreateTransitionAnimationSettings(
        view_shadow_->shadow()->layer());
    view_shadow_->shadow()->layer()->SetTransform(gfx::Transform());
  }
}

void SearchResultPageView::OnAnimationUpdated(double progress,
                                              ash::AppListState from_state,
                                              ash::AppListState to_state) {
  if (from_state != ash::AppListState::kStateSearchResults &&
      to_state != ash::AppListState::kStateSearchResults) {
    return;
  }
  const SearchBoxView* search_box =
      AppListPage::contents_view()->GetSearchBoxView();
  const SkColor color = gfx::Tween::ColorValueBetween(
      progress, search_box->GetBackgroundColorForState(from_state),
      search_box->GetBackgroundColorForState(to_state));

  if (color != background()->get_color()) {
    background()->SetNativeControlColor(color);
    SchedulePaint();
  }
}

gfx::Size SearchResultPageView::GetPreferredSearchBoxSize() const {
  static gfx::Size size = gfx::Size(kWidth, kSearchBoxHeight);
  return size;
}

base::Optional<int> SearchResultPageView::GetSearchBoxTop(
    ash::AppListViewState view_state) const {
  if (view_state == ash::AppListViewState::kPeeking ||
      view_state == ash::AppListViewState::kHalf) {
    return AppListConfig::instance().search_box_fullscreen_top_padding();
  }
  // For other view states, return base::nullopt so the app_list::ContentsView
  // sets the default search box widget origin.
  return base::nullopt;
}

gfx::Rect SearchResultPageView::GetPageBoundsForState(
    ash::AppListState state,
    const gfx::Rect& contents_bounds,
    const gfx::Rect& search_box_bounds) const {
  if (state != ash::AppListState::kStateSearchResults) {
    // Hides this view behind the search box by using the same bounds.
    return search_box_bounds;
  }

  gfx::Rect bounds = contents_bounds;
  bounds.Offset((contents_bounds.width() - kWidth) / 2, search_box_bounds.y());
  bounds.set_size(GetPreferredSize());
  return bounds;
}

views::View* SearchResultPageView::GetFirstFocusableView() {
  return GetFocusManager()->GetNextFocusableView(
      this, GetWidget(), false /* reverse */, false /* dont_loop */);
}

views::View* SearchResultPageView::GetLastFocusableView() {
  return GetFocusManager()->GetNextFocusableView(
      this, GetWidget(), true /* reverse */, false /* dont_loop */);
}

}  // namespace app_list
