// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/contents_view.h"

#include <algorithm>
#include <vector>

#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/app_list/views/assistant/assistant_page_view.h"
#include "ash/app_list/views/expand_arrow_view.h"
#include "ash/app_list/views/horizontal_page_container.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_answer_card_view.h"
#include "ash/app_list/views/search_result_list_view.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/app_list/views/search_result_tile_item_list_view.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "base/logging.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view_model.h"
#include "ui/views/widget/widget.h"

namespace app_list {

namespace {

// The range of app list transition progress in which the expand arrow'
// opacity changes from 0 to 1.
constexpr float kExpandArrowOpacityStartProgress = 0.61;
constexpr float kExpandArrowOpacityEndProgress = 1;

void DoAnimation(base::TimeDelta animation_duration,
                 ui::Layer* layer,
                 float target_opacity) {
  ui::ScopedLayerAnimationSettings animation(layer->GetAnimator());
  animation.SetTransitionDuration(animation_duration);
  animation.SetTweenType(gfx::Tween::EASE_IN);
  animation.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  layer->SetOpacity(target_opacity);
}

}  // namespace

ContentsView::ContentsView(AppListView* app_list_view)
    : app_list_view_(app_list_view) {
  pagination_model_.SetTransitionDurations(
      AppListConfig::instance().page_transition_duration_ms(),
      AppListConfig::instance().overscroll_page_transition_duration_ms());
  pagination_model_.AddObserver(this);
}

ContentsView::~ContentsView() {
  pagination_model_.RemoveObserver(this);
}

void ContentsView::Init(AppListModel* model) {
  DCHECK(model);
  model_ = model;

  AppListViewDelegate* view_delegate = GetAppListMainView()->view_delegate();

  horizontal_page_container_ = new HorizontalPageContainer(this, model);

  AddLauncherPage(horizontal_page_container_, ash::AppListState::kStateApps);

  // Search results UI.
  search_results_page_view_ = new SearchResultPageView(view_delegate);

  // Search result containers.
  SearchModel::SearchResults* results =
      view_delegate->GetSearchModel()->results();

  if (app_list_features::IsAnswerCardEnabled()) {
    search_result_answer_card_view_ =
        new SearchResultAnswerCardView(view_delegate);
    search_results_page_view_->AddSearchResultContainerView(
        results, search_result_answer_card_view_);
  }

  expand_arrow_view_ = new ExpandArrowView(this, app_list_view_);
  AddChildView(expand_arrow_view_);

  search_result_tile_item_list_view_ = new SearchResultTileItemListView(
      search_results_page_view_, GetSearchBoxView()->search_box(),
      view_delegate);
  search_results_page_view_->AddSearchResultContainerView(
      results, search_result_tile_item_list_view_);

  search_result_list_view_ =
      new SearchResultListView(GetAppListMainView(), view_delegate);
  search_results_page_view_->AddSearchResultContainerView(
      results, search_result_list_view_);

  AddLauncherPage(search_results_page_view_,
                  ash::AppListState::kStateSearchResults);

  if (app_list_features::IsEmbeddedAssistantUIEnabled()) {
    assistant_page_view_ =
        new AssistantPageView(view_delegate->GetAssistantViewDelegate(), this);
    assistant_page_view_->SetVisible(false);
    AddLauncherPage(assistant_page_view_,
                    ash::AppListState::kStateEmbeddedAssistant);
  }

  int initial_page_index = GetPageIndexForState(ash::AppListState::kStateApps);
  DCHECK_GE(initial_page_index, 0);

  page_before_search_ = initial_page_index;
  page_before_assistant_ = initial_page_index;
  // Must only call SetTotalPages once all the launcher pages have been added
  // (as it will trigger a SelectedPageChanged call).
  pagination_model_.SetTotalPages(app_list_pages_.size());

  // Page 0 is selected by SetTotalPages and needs to be 'hidden' when selecting
  // the initial page.
  app_list_pages_[GetActivePageIndex()]->OnWillBeHidden();

  pagination_model_.SelectPage(initial_page_index, false);

  // Update suggestion chips after valid page is selected to prevent the update
  // from being ignored.
  GetAppsContainerView()->UpdateSuggestionChips();

  ActivePageChanged();

  // Hide the search results initially.
  ShowSearchResults(false);
}

void ContentsView::ResetForShow() {
  GetAppsContainerView()->ResetForShowApps();
  // SearchBoxView::ResetForShow() before SetActiveState(). It clears the search
  // query internally, which can show the search results page through
  // QueryChanged(). Since it wants to reset to kStateApps, first reset the
  // search box and then set its active state to kStateApps.
  GetSearchBoxView()->ResetForShow();
  SetActiveState(ash::AppListState::kStateApps, /*animate=*/false);
  // Make other pages invisible.
  search_results_page_view_->SetVisible(false);
  if (assistant_page_view_)
    assistant_page_view_->SetVisible(false);
  // In side shelf, the opacity of the contents is not animated so set it to the
  // final state. In tablet mode, opacity of the elements is controlled by the
  // HomeLauncherGestureHandler which expects these elements to be opaque.
  // Otherwise the contents animate from 0 to 1 so set the initial opacity to 0.
  const float initial_opacity =
      app_list_view_->is_side_shelf() || app_list_view_->is_tablet_mode()
          ? 1.0f
          : 0.0f;
  GetSearchBoxView()->layer()->SetOpacity(initial_opacity);
  layer()->SetOpacity(initial_opacity);
}

void ContentsView::CancelDrag() {
  if (GetAppsContainerView()->apps_grid_view()->has_dragged_view())
    GetAppsContainerView()->apps_grid_view()->EndDrag(true);
  if (GetAppsContainerView()
          ->app_list_folder_view()
          ->items_grid_view()
          ->has_dragged_view()) {
    GetAppsContainerView()->app_list_folder_view()->items_grid_view()->EndDrag(
        true);
  }
}

void ContentsView::SetDragAndDropHostOfCurrentAppList(
    ApplicationDragAndDropHost* drag_and_drop_host) {
  GetAppsContainerView()->SetDragAndDropHostOfCurrentAppList(
      drag_and_drop_host);
}

void ContentsView::OnAppListViewTargetStateChanged(
    ash::AppListViewState target_state) {
  target_view_state_ = target_state;
  if (target_state == ash::AppListViewState::kClosed) {
    CancelDrag();
    expand_arrow_view_->MaybeEnableHintingAnimation(false);
    return;
  }
  UpdateExpandArrowBehavior(target_state);
}

void ContentsView::SetActiveState(ash::AppListState state) {
  SetActiveState(state, !AppListView::ShortAnimationsForTesting());
}

void ContentsView::SetActiveState(ash::AppListState state, bool animate) {
  if (IsStateActive(state))
    return;

  // The primary way to set the state to search or Assistant results should be
  // via |ShowSearchResults| or |ShowEmbeddedAssistantUI|.
  DCHECK(state != ash::AppListState::kStateSearchResults &&
         state != ash::AppListState::kStateEmbeddedAssistant);

  const int page_index = GetPageIndexForState(state);
  page_before_search_ = page_index;
  page_before_assistant_ = page_index;
  SetActiveStateInternal(page_index, animate);
}

int ContentsView::GetActivePageIndex() const {
  // The active page is changed at the beginning of an animation, not the end.
  return pagination_model_.SelectedTargetPage();
}

ash::AppListState ContentsView::GetActiveState() const {
  return GetStateForPageIndex(GetActivePageIndex());
}

bool ContentsView::IsStateActive(ash::AppListState state) const {
  int active_page_index = GetActivePageIndex();
  return active_page_index >= 0 &&
         GetPageIndexForState(state) == active_page_index;
}

int ContentsView::GetPageIndexForState(ash::AppListState state) const {
  // Find the index of the view corresponding to the given state.
  std::map<ash::AppListState, int>::const_iterator it =
      state_to_view_.find(state);
  if (it == state_to_view_.end())
    return -1;

  return it->second;
}

ash::AppListState ContentsView::GetStateForPageIndex(int index) const {
  std::map<int, ash::AppListState>::const_iterator it =
      view_to_state_.find(index);
  if (it == view_to_state_.end())
    return ash::AppListState::kInvalidState;

  return it->second;
}

int ContentsView::NumLauncherPages() const {
  return pagination_model_.total_pages();
}

AppsContainerView* ContentsView::GetAppsContainerView() {
  return horizontal_page_container_->apps_container_view();
}

void ContentsView::SetActiveStateInternal(int page_index, bool animate) {
  if (!GetPageView(page_index)->GetVisible())
    return;

  app_list_pages_[GetActivePageIndex()]->OnWillBeHidden();

  // Start animating to the new page.
  const bool should_animate = animate && !set_active_state_without_animation_;
  // There's a chance of selecting page during the transition animation. To
  // reschedule the new animation from the beginning, |pagination_model_| needs
  // to finish the ongoing animation here.
  if (should_animate && pagination_model_.has_transition() &&
      pagination_model_.transition().target_page != page_index) {
    pagination_model_.FinishAnimation();
  }
  pagination_model_.SelectPage(page_index, should_animate);
  ActivePageChanged();

  if (!should_animate)
    Layout();
}

void ContentsView::ActivePageChanged() {
  ash::AppListState state = ash::AppListState::kInvalidState;

  std::map<int, ash::AppListState>::const_iterator it =
      view_to_state_.find(GetActivePageIndex());
  if (it != view_to_state_.end())
    state = it->second;

  app_list_pages_[GetActivePageIndex()]->OnWillBeShown();

  GetAppListMainView()->model()->SetState(state);
  UpdateSearchBoxVisibility(state);
}

void ContentsView::ShowSearchResults(bool show) {
  int search_page =
      GetPageIndexForState(ash::AppListState::kStateSearchResults);
  DCHECK_GE(search_page, 0);

  // Hide or Show results
  GetPageView(search_page)->SetVisible(show);

  page_before_assistant_ = show ? search_page : page_before_search_;
  SetActiveStateInternal(show ? search_page : page_before_search_,
                         !AppListView::ShortAnimationsForTesting());
}

bool ContentsView::IsShowingSearchResults() const {
  return IsStateActive(ash::AppListState::kStateSearchResults);
}

void ContentsView::ShowEmbeddedAssistantUI(bool show) {
  const int assistant_page =
      GetPageIndexForState(ash::AppListState::kStateEmbeddedAssistant);
  DCHECK_GE(assistant_page, 0);

  // Hide or Show results.
  auto* page_view = GetPageView(assistant_page);
  page_view->SetVisible(show);
  if (show)
    page_view->RequestFocus();

  const int search_results_page =
      GetPageIndexForState(ash::AppListState::kStateSearchResults);
  DCHECK_GE(search_results_page, 0);
  GetPageView(page_before_assistant_)->SetVisible(!show);

  // No animation when transiting from/to |search_results_page| and in test.
  const bool animate = !AppListView::ShortAnimationsForTesting() &&
                       page_before_assistant_ != search_results_page;
  const int current_page = pagination_model_.selected_page();
  SetActiveStateInternal(show ? assistant_page : page_before_assistant_,
                         animate);
  // Sometimes the page stays in |assistant_page|, but the preferred bounds
  // might change meanwhile.
  if (show && current_page == assistant_page) {
    page_view->SetBoundsRect(page_view->GetPageBoundsForState(
        ash::AppListState::kStateEmbeddedAssistant));
  }
  // If |page_before_assistant_| is kStateApps, we need to set app_list_view to
  // kPeeking and layout the suggestion chips.
  if (!show && page_before_assistant_ ==
                   GetPageIndexForState(ash::AppListState::kStateApps)) {
    GetSearchBoxView()->ClearSearch();
    GetSearchBoxView()->SetSearchBoxActive(false, ui::ET_UNKNOWN);
    GetAppsContainerView()->Layout();
  }
}

bool ContentsView::IsShowingEmbeddedAssistantUI() const {
  return IsStateActive(ash::AppListState::kStateEmbeddedAssistant);
}

void ContentsView::InitializeSearchBoxAnimation(
    ash::AppListState current_state,
    ash::AppListState target_state) {
  SearchBoxView* search_box = GetSearchBoxView();
  if (!search_box->GetWidget())
    return;

  search_box->UpdateLayout(1.f, current_state, target_state);
  search_box->UpdateBackground(1.f, current_state, target_state);

  gfx::Rect target_bounds = GetSearchBoxBoundsForState(target_state);
  target_bounds = search_box->GetViewBoundsForSearchBoxContentsBounds(
      ConvertRectToWidgetWithoutTransform(target_bounds));

  // The search box animation is conducted as transform animation. Initially
  // search box changes its bounds to the target bounds but sets the transform
  // to be original bounds. Note that this transform shouldn't be animated
  // through ui::LayerAnimator since intermediate transformed bounds might not
  // match with other animation and that could look janky.
  search_box->GetWidget()->SetBounds(target_bounds);

  UpdateSearchBoxAnimation(0.0f, current_state, target_state);
}

void ContentsView::UpdateSearchBoxAnimation(double progress,
                                            ash::AppListState current_state,
                                            ash::AppListState target_state) {
  SearchBoxView* search_box = GetSearchBoxView();
  if (!search_box->GetWidget())
    return;

  gfx::Rect previous_bounds = GetSearchBoxBoundsForState(current_state);
  previous_bounds = search_box->GetViewBoundsForSearchBoxContentsBounds(
      ConvertRectToWidgetWithoutTransform(previous_bounds));
  gfx::Rect target_bounds = GetSearchBoxBoundsForState(target_state);
  target_bounds = search_box->GetViewBoundsForSearchBoxContentsBounds(
      ConvertRectToWidgetWithoutTransform(target_bounds));

  gfx::Rect current_bounds =
      gfx::Tween::RectValueBetween(progress, previous_bounds, target_bounds);
  gfx::Transform transform;

  if (current_bounds != target_bounds) {
    transform.Translate(current_bounds.origin() - target_bounds.origin());
    transform.Scale(
        static_cast<float>(current_bounds.width()) / target_bounds.width(),
        static_cast<float>(current_bounds.height()) / target_bounds.height());
  }
  search_box->GetWidget()->GetLayer()->SetTransform(transform);
}

void ContentsView::UpdateExpandArrowOpacity(ash::AppListState target_state,
                                            bool animate) {
  float expand_arrow_target_opacity = 0.0f;
  if (target_state == ash::AppListState::kStateApps) {
    expand_arrow_target_opacity = 1.0f;
  } else if (target_state == ash::AppListState::kStateSearchResults ||
             target_state == ash::AppListState::kStateEmbeddedAssistant) {
    expand_arrow_target_opacity = 0.0f;
  } else {
    // not updated.
    return;
  }

  std::unique_ptr<ui::ScopedLayerAnimationSettings> settings;
  if (animate)
    settings = CreateTransitionAnimationSettings(expand_arrow_view_->layer());
  expand_arrow_view_->layer()->SetOpacity(expand_arrow_target_opacity);
}

void ContentsView::UpdateExpandArrowBehavior(
    ash::AppListViewState target_state) {
  const bool expand_arrow_enabled =
      target_state == ash::AppListViewState::kPeeking;
  // The expand arrow is only focusable and has InkDropMode on in peeking
  // state.
  expand_arrow_view_->SetFocusBehavior(
      expand_arrow_enabled ? FocusBehavior::ALWAYS : FocusBehavior::NEVER);
  expand_arrow_view_->SetInkDropMode(
      expand_arrow_enabled ? views::InkDropHostView::InkDropMode::ON
                           : views::InkDropHostView::InkDropMode::OFF);

  // Allow ChromeVox to focus the expand arrow only when peeking launcher.
  expand_arrow_view_->GetViewAccessibility().OverrideIsIgnored(
      !expand_arrow_enabled);
  expand_arrow_view_->GetViewAccessibility().NotifyAccessibilityEvent(
      ax::mojom::Event::kTreeChanged);

  expand_arrow_view_->MaybeEnableHintingAnimation(expand_arrow_enabled);
}

void ContentsView::UpdateSearchBoxVisibility(ash::AppListState current_state) {
  auto* search_box_widget = GetSearchBoxView()->GetWidget();
  if (search_box_widget) {
    // Hide search box widget in order to click on the embedded Assistant UI.
    const bool show_search_box =
        current_state != ash::AppListState::kStateEmbeddedAssistant;
    show_search_box ? search_box_widget->Show() : search_box_widget->Hide();
  }
}

ash::PaginationModel* ContentsView::GetAppsPaginationModel() {
  return GetAppsContainerView()->apps_grid_view()->pagination_model();
}

void ContentsView::ShowFolderContent(AppListFolderItem* item) {
  GetAppsContainerView()->ShowActiveFolder(item);
}

AppListPage* ContentsView::GetPageView(int index) const {
  DCHECK_GT(static_cast<int>(app_list_pages_.size()), index);
  return app_list_pages_[index];
}

SearchBoxView* ContentsView::GetSearchBoxView() const {
  return GetAppListMainView()->search_box_view();
}

AppListMainView* ContentsView::GetAppListMainView() const {
  return app_list_view_->app_list_main_view();
}

int ContentsView::AddLauncherPage(AppListPage* view) {
  view->set_contents_view(this);
  AddChildView(view);
  app_list_pages_.push_back(view);
  return app_list_pages_.size() - 1;
}

int ContentsView::AddLauncherPage(AppListPage* view, ash::AppListState state) {
  int page_index = AddLauncherPage(view);
  bool success =
      state_to_view_.insert(std::make_pair(state, page_index)).second;
  success = success &&
            view_to_state_.insert(std::make_pair(page_index, state)).second;

  // There shouldn't be duplicates in either map.
  DCHECK(success);
  return page_index;
}

gfx::Rect ContentsView::GetDefaultSearchBoxBounds() const {
  gfx::Rect search_box_bounds;
  search_box_bounds.set_size(GetSearchBoxView()->GetPreferredSize());
  search_box_bounds.Offset((bounds().width() - search_box_bounds.width()) / 2,
                           0);
  search_box_bounds.set_y(
      AppListConfig::instance().search_box_fullscreen_top_padding());
  // Reduce the search box size in fullscreen view state when the work area
  // height is less than 600 dip - the goal is to increase the amount of space
  // available to apps grid.
  if (GetContentsBounds().height() < 600 &&
      !app_list_features::IsScalableAppListEnabled() &&
      (target_view_state_ == ash::AppListViewState::kFullscreenAllApps ||
       target_view_state_ == ash::AppListViewState::kFullscreenSearch)) {
    search_box_bounds.set_height(40);
  }
  return search_box_bounds;
}

gfx::Rect ContentsView::GetSearchBoxBoundsForState(
    ash::AppListState state) const {
  AppListPage* page = GetPageView(GetPageIndexForState(state));
  return page->GetSearchBoxBoundsForState(state);
}

gfx::Rect ContentsView::GetDefaultContentsBounds() const {
  return GetContentsBounds();
}

bool ContentsView::Back() {
  // If the virtual keyboard is visible, dismiss the keyboard and return early
  auto* const keyboard_controller = keyboard::KeyboardUIController::Get();
  if (keyboard_controller->IsKeyboardVisible()) {
    keyboard_controller->HideKeyboardByUser();
    return true;
  }

  ash::AppListState state = view_to_state_[GetActivePageIndex()];
  switch (state) {
    case ash::AppListState::kStateApps: {
      ash::PaginationModel* pagination_model =
          GetAppsContainerView()->apps_grid_view()->pagination_model();
      if (GetAppsContainerView()->IsInFolderView()) {
        GetAppsContainerView()->app_list_folder_view()->CloseFolderPage();
      } else if (app_list_view_->is_tablet_mode() &&
                 pagination_model->total_pages() > 0 &&
                 pagination_model->selected_page() > 0) {
        pagination_model->SelectPage(
            0, !app_list_view_->ShortAnimationsForTesting());
      } else {
        // Close the app list when Back() is called from the apps page.
        return false;
      }
      break;
    }
    case ash::AppListState::kStateSearchResults:
      GetSearchBoxView()->ClearSearchAndDeactivateSearchBox();
      ShowSearchResults(false);
      for (auto& observer : search_box_observers_)
        observer.OnSearchBoxClearAndDeactivated();
      break;
    case ash::AppListState::kStateEmbeddedAssistant:
      ShowEmbeddedAssistantUI(false);
      break;
    case ash::AppListState::kStateStart_DEPRECATED:
    case ash::AppListState::kInvalidState:
      NOTREACHED();
      break;
  }
  return true;
}

gfx::Size ContentsView::GetDefaultContentsSize() const {
  return horizontal_page_container_->GetPreferredSize();
}

void ContentsView::Layout() {
  const gfx::Rect rect = GetContentsBounds();
  if (rect.IsEmpty())
    return;

  // Layout expand arrow.
  gfx::Rect arrow_rect(GetContentsBounds());
  const gfx::Size arrow_size(expand_arrow_view_->GetPreferredSize());
  arrow_rect.set_height(arrow_size.height());
  arrow_rect.ClampToCenteredSize(arrow_size);
  expand_arrow_view_->SetBoundsRect(arrow_rect);
  expand_arrow_view_->SchedulePaint();

  if (pagination_model_.has_transition())
    return;

  // The bounds calculations will potentially be mid-transition (depending on
  // the state of the PaginationModel).
  int current_page = std::max(0, pagination_model_.selected_page());
  ash::AppListState current_state = GetStateForPageIndex(current_page);
  // Update app list pages.
  for (AppListPage* page : app_list_pages_)
    page->SetBoundsRect(page->GetPageBoundsForState(current_state));

  UpdateExpandArrowOpacity(current_state, false);

  // Update the searchbox bounds.
  auto* search_box = GetSearchBoxView();
  gfx::Rect search_box_bounds = GetSearchBoxBoundsForState(current_state);
  search_box_bounds = search_box->GetViewBoundsForSearchBoxContentsBounds(
      ConvertRectToWidgetWithoutTransform(search_box_bounds));
  search_box->GetWidget()->SetBounds(search_box_bounds);
  search_box->UpdateLayout(1.f, current_state, current_state);
  search_box->UpdateBackground(1.f, current_state, current_state);
  // Reset the transform which can be set through animation.
  search_box->GetWidget()->GetLayer()->SetTransform(gfx::Transform());
}

const char* ContentsView::GetClassName() const {
  return "ContentsView";
}

void ContentsView::TotalPagesChanged() {}

void ContentsView::SelectedPageChanged(int old_selected, int new_selected) {
  if (old_selected >= 0)
    app_list_pages_[old_selected]->OnHidden();

  if (new_selected >= 0)
    app_list_pages_[new_selected]->OnShown();
}

void ContentsView::TransitionStarted() {
  const int current_page = pagination_model_.selected_page();
  const int target_page = pagination_model_.transition().target_page;

  const ash::AppListState current_state = GetStateForPageIndex(current_page);
  const ash::AppListState target_state = GetStateForPageIndex(target_page);
  for (AppListPage* page : app_list_pages_)
    page->OnAnimationStarted(current_state, target_state);

  InitializeSearchBoxAnimation(current_state, target_state);
  UpdateExpandArrowOpacity(target_state, true);
}

void ContentsView::TransitionChanged() {
  const int current_page = pagination_model_.selected_page();
  const int target_page = pagination_model_.transition().target_page;

  const ash::AppListState current_state = GetStateForPageIndex(current_page);
  const ash::AppListState target_state = GetStateForPageIndex(target_page);
  const double progress = pagination_model_.transition().progress;
  for (AppListPage* page : app_list_pages_) {
    if (!page->GetVisible() ||
        !ShouldLayoutPage(page, current_state, target_state)) {
      continue;
    }
    page->OnAnimationUpdated(progress, current_state, target_state);
  }

  // Update search box's transform gradually. See the comment in
  // InitiateSearchBoxAnimation for why it's not animated through
  // ui::LayerAnimator.
  UpdateSearchBoxAnimation(progress, current_state, target_state);
}

void ContentsView::FadeOutOnClose(base::TimeDelta animation_duration) {
  DoAnimation(animation_duration, layer(), 0.0f);
  DoAnimation(animation_duration, GetSearchBoxView()->layer(), 0.0f);
}

void ContentsView::FadeInOnOpen(base::TimeDelta animation_duration) {
  if (layer()->opacity() == 1.0f &&
      GetSearchBoxView()->layer()->opacity() == 1.0f) {
    return;
  }
  DoAnimation(animation_duration, layer(), 1.0f);
  DoAnimation(animation_duration, GetSearchBoxView()->layer(), 1.0f);
}

views::View* ContentsView::GetSelectedView() const {
  return app_list_pages_[GetActivePageIndex()]->GetSelectedView();
}

void ContentsView::UpdateYPositionAndOpacity() {
  ash::AppListViewState state = app_list_view_->app_list_state();
  if (state == ash::AppListViewState::kFullscreenSearch ||
      state == ash::AppListViewState::kHalf) {
    return;
  }

  const bool should_restore_opacity =
      !app_list_view_->is_in_drag() &&
      (app_list_view_->app_list_state() != ash::AppListViewState::kClosed);

  // Changes the opacity of expand arrow between 0 and 1 when app list
  // transition progress changes between |kExpandArrowOpacityStartProgress|
  // and |kExpandArrowOpacityEndProgress|.
  expand_arrow_view_->layer()->SetOpacity(
      should_restore_opacity
          ? 1.0f
          : std::min(std::max((app_list_view_->GetAppListTransitionProgress() -
                               kExpandArrowOpacityStartProgress) /
                                  (kExpandArrowOpacityEndProgress -
                                   kExpandArrowOpacityStartProgress),
                              0.f),
                     1.0f));

  expand_arrow_view_->SchedulePaint();

  AppsContainerView* apps_container_view = GetAppsContainerView();
  SearchBoxView* search_box = GetSearchBoxView();
  gfx::Rect search_rect = search_box->GetViewBoundsForSearchBoxContentsBounds(
      ConvertRectToWidgetWithoutTransform(
          apps_container_view->GetSearchBoxExpectedBounds()));
  search_box->GetWidget()->SetBounds(search_rect);

  search_results_page_view()->SetBoundsRect(
      apps_container_view->GetSearchBoxExpectedBounds());

  apps_container_view->UpdateYPositionAndOpacity();
}

void ContentsView::SetExpandArrowViewVisibility(bool show) {
  if (expand_arrow_view_->GetVisible() == show)
    return;

  expand_arrow_view_->SetVisible(show);
}

std::unique_ptr<ui::ScopedLayerAnimationSettings>
ContentsView::CreateTransitionAnimationSettings(ui::Layer* layer) const {
  DCHECK(pagination_model_.has_transition());
  auto settings =
      std::make_unique<ui::ScopedLayerAnimationSettings>(layer->GetAnimator());
  settings->SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
  settings->SetTransitionDuration(
      pagination_model_.GetTransitionAnimationSlideDuration());
  return settings;
}

void ContentsView::NotifySearchBoxBoundsUpdated() {
  for (auto& observer : search_box_observers_)
    observer.OnSearchBoxBoundsUpdated();
}

void ContentsView::AddSearchBoxUpdateObserver(
    SearchBoxUpdateObserver* observer) {
  search_box_observers_.AddObserver(observer);
}

void ContentsView::RemoveSearchBoxUpdateObserver(
    SearchBoxUpdateObserver* observer) {
  search_box_observers_.RemoveObserver(observer);
}

bool ContentsView::ShouldLayoutPage(AppListPage* page,
                                    ash::AppListState current_state,
                                    ash::AppListState target_state) const {
  if (page == horizontal_page_container_) {
    return (current_state == ash::AppListState::kStateSearchResults &&
            target_state == ash::AppListState::kStateApps);
  }

  if (page == search_results_page_view_) {
    return ((current_state == ash::AppListState::kStateSearchResults &&
             target_state == ash::AppListState::kStateApps) ||
            (current_state == ash::AppListState::kStateApps &&
             target_state == ash::AppListState::kStateSearchResults));
  }

  if (page == assistant_page_view_) {
    return current_state == ash::AppListState::kStateEmbeddedAssistant ||
           target_state == ash::AppListState::kStateEmbeddedAssistant;
  }

  return false;
}

gfx::Rect ContentsView::ConvertRectToWidgetWithoutTransform(
    const gfx::Rect& rect) {
  gfx::Rect widget_rect = rect;
  for (const views::View* v = this; v; v = v->parent()) {
    widget_rect.Offset(v->GetMirroredPosition().OffsetFromOrigin());
  }
  return widget_rect;
}

}  // namespace app_list
