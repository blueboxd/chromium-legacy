// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/horizontal_page_container.h"

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/pagination/pagination_controller.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/chromeos/search_box/search_box_constants.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/controls/label.h"

namespace app_list {

HorizontalPageContainer::HorizontalPageContainer(ContentsView* contents_view,
                                                 AppListModel* model)
    : contents_view_(contents_view) {
  // Assumes all horizontal pages paint to their own layers.
  SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  pagination_model_.SetTransitionDurations(
      AppListConfig::instance().page_transition_duration(),
      AppListConfig::instance().overscroll_page_transition_duration());
  pagination_model_.AddObserver(this);
  pagination_controller_ = std::make_unique<ash::PaginationController>(
      &pagination_model_, ash::PaginationController::SCROLL_AXIS_HORIZONTAL,
      base::BindRepeating(&RecordPageSwitcherSourceByEventType),
      contents_view_->app_list_view()->is_tablet_mode());

  // Add horizontal pages.
  apps_container_view_ = new AppsContainerView(contents_view_, model);
  AddHorizontalPage(apps_container_view_);
  pagination_model_.SetTotalPages(horizontal_pages_.size());

  // By default select apps container page.
  pagination_model_.SelectPage(GetIndexForPage(apps_container_view_), false);
}

HorizontalPageContainer::~HorizontalPageContainer() {
  pagination_model_.RemoveObserver(this);
}

gfx::Size HorizontalPageContainer::CalculatePreferredSize() const {
  if (!GetWidget())
    return gfx::Size();

  return contents_view_->GetContentsBounds().size();
}

void HorizontalPageContainer::Layout() {
  if (!GetWidget())
    return;

  for (size_t i = 0; i < horizontal_pages_.size(); ++i) {
    HorizontalPage* page = horizontal_pages_[i];
    gfx::Rect page_bounds(
        page->GetPageBoundsForState(contents_view_->GetActiveState()));
    page_bounds.Offset(GetOffsetForPageIndex(i));
    page->SetBoundsRect(page_bounds);
  }
}

void HorizontalPageContainer::OnGestureEvent(ui::GestureEvent* event) {
  if (pagination_controller_->OnGestureEvent(*event, GetContentsBounds()))
    event->SetHandled();
}

const char* HorizontalPageContainer::GetClassName() const {
  return "HorizontalPageContainer";
}

void HorizontalPageContainer::OnWillBeHidden() {
  GetSelectedPage()->OnWillBeHidden();
}

void HorizontalPageContainer::OnAnimationStarted(ash::AppListState from_state,
                                                 ash::AppListState to_state) {
  gfx::Rect contents_bounds = GetDefaultContentsBounds();

  const gfx::Rect from_rect =
      GetPageBoundsForState(from_state, contents_bounds, gfx::Rect());
  const gfx::Rect to_rect =
      GetPageBoundsForState(to_state, contents_bounds, gfx::Rect());
  if (from_rect != to_rect) {
    SetBoundsRect(from_rect);
    auto settings = contents_view()->CreateTransitionAnimationSettings(layer());
    SetBoundsRect(to_rect);
  }

  for (size_t i = 0; i < horizontal_pages_.size(); ++i) {
    HorizontalPage* page = horizontal_pages_[i];
    gfx::Rect page_to_rect = page->GetPageBoundsForState(to_state);
    gfx::Rect page_from_rect = page->GetPageBoundsForState(from_state);

    if (page_to_rect == page_from_rect)
      continue;

    page_to_rect.Offset(GetOffsetForPageIndex(i));

    auto settings =
        contents_view()->CreateTransitionAnimationSettings(page->layer());
    page->SetBoundsRect(page_to_rect);
  }
}

gfx::Rect HorizontalPageContainer::GetPageBoundsForState(
    ash::AppListState state,
    const gfx::Rect& contents_bounds,
    const gfx::Rect& search_box_bounds) const {
  if (state == ash::AppListState::kStateApps)
    return contents_bounds;
  return GetBelowContentsOffscreenBounds(contents_bounds.size());
}

views::View* HorizontalPageContainer::GetFirstFocusableView() {
  return GetSelectedPage()->GetFirstFocusableView();
}

views::View* HorizontalPageContainer::GetLastFocusableView() {
  return GetSelectedPage()->GetLastFocusableView();
}

bool HorizontalPageContainer::ShouldShowSearchBox() const {
  return GetSelectedPage()->ShouldShowSearchBox();
}

void HorizontalPageContainer::OnTabletModeChanged(bool started) {
  pagination_controller_->set_is_tablet_mode(started);
}

void HorizontalPageContainer::TotalPagesChanged() {}

void HorizontalPageContainer::SelectedPageChanged(int old_selected,
                                                  int new_selected) {
  Layout();
}

void HorizontalPageContainer::TransitionStarting() {
  Layout();
}

void HorizontalPageContainer::TransitionChanged() {
  Layout();

  // Transition the search box opacity.
  const int current_page = pagination_model_.selected_page();
  DCHECK(pagination_model_.is_valid_page(current_page));
  const ash::PaginationModel::Transition& transition =
      pagination_model_.transition();
  const bool is_valid = pagination_model_.is_valid_page(transition.target_page);
  float search_box_opacity =
      GetSelectedPage()->ShouldShowSearchBox() ? 1.0f : 0.0f;
  if (is_valid) {
    float target_search_box_opacity =
        horizontal_pages_[transition.target_page]->ShouldShowSearchBox() ? 1.0f
                                                                         : 0.0f;
    search_box_opacity = gfx::Tween::FloatValueBetween(
        transition.progress, search_box_opacity, target_search_box_opacity);
  }
  contents_view_->GetSearchBoxView()->layer()->SetOpacity(search_box_opacity);
  contents_view_->search_results_page_view()->layer()->SetOpacity(
      search_box_opacity);
}

void HorizontalPageContainer::TransitionEnded() {
  Layout();
}

int HorizontalPageContainer::AddHorizontalPage(HorizontalPage* view) {
  AddChildView(view);
  horizontal_pages_.emplace_back(view);
  return horizontal_pages_.size() - 1;
}

int HorizontalPageContainer::GetIndexForPage(HorizontalPage* view) const {
  for (size_t i = 0; i < horizontal_pages_.size(); ++i) {
    if (horizontal_pages_[i] == view)
      return i;
  }
  return -1;
}

HorizontalPage* HorizontalPageContainer::GetSelectedPage() {
  return const_cast<HorizontalPage*>(
      const_cast<const HorizontalPageContainer*>(this)->GetSelectedPage());
}

const HorizontalPage* HorizontalPageContainer::GetSelectedPage() const {
  const int current_page = pagination_model_.selected_page();
  DCHECK(pagination_model_.is_valid_page(current_page));
  return horizontal_pages_[current_page];
}

gfx::Vector2d HorizontalPageContainer::GetOffsetForPageIndex(int index) const {
  const int current_page = pagination_model_.selected_page();
  DCHECK(pagination_model_.is_valid_page(current_page));
  const ash::PaginationModel::Transition& transition =
      pagination_model_.transition();
  const bool is_valid = pagination_model_.is_valid_page(transition.target_page);
  const int dir = transition.target_page > current_page ? -1 : 1;
  int x_offset = 0;
  // TODO(https://crbug.com/820510): figure out the right pagination style.
  if (index < current_page)
    x_offset = -width();
  else if (index > current_page)
    x_offset = width();

  if (is_valid) {
    if (index == current_page || index == transition.target_page) {
      x_offset += transition.progress * width() * dir;
    }
  }
  return gfx::Vector2d(x_offset, 0);
}

}  // namespace app_list
