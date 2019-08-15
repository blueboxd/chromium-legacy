// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_layout_helper.h"

#include <algorithm>
#include <memory>
#include <set>
#include <utility>

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_animation.h"
#include "chrome/browser/ui/views/tabs/tab_animation_state.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_layout.h"
#include "chrome/browser/ui/views/tabs/tab_strip_layout_types.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/views/view_model.h"

namespace {

// TODO(965227): Align animation ticks to compositor events.
constexpr base::TimeDelta kTickInterval =
    base::TimeDelta::FromMilliseconds(1000 / 60.0);

// The types of TabSlotView that can be referenced by TabSlot.
enum class ViewType {
  kTab,
  kGroupHeader,
};

TabLayoutConstants GetTabLayoutConstants() {
  return {GetLayoutConstant(TAB_HEIGHT), TabStyle::GetTabOverlap()};
}

}  // namespace

struct TabStripLayoutHelper::TabSlot {
  static TabStripLayoutHelper::TabSlot CreateForTab(
      Tab* tab,
      TabAnimationState::TabOpenness openness,
      TabAnimationState::TabPinnedness pinned,
      base::OnceClosure removed_callback) {
    TabStripLayoutHelper::TabSlot slot;
    slot.type = ViewType::kTab;
    slot.view = tab;
    TabAnimationState initial_state = TabAnimationState::ForIdealTabState(
        openness, pinned, TabAnimationState::TabActiveness::kInactive, 0);
    slot.animation = std::make_unique<TabAnimation>(
        initial_state, std::move(removed_callback));
    return slot;
  }

  static TabStripLayoutHelper::TabSlot CreateForGroupHeader(
      TabGroupId group,
      TabGroupHeader* header,
      TabAnimationState::TabPinnedness pinned,
      base::OnceClosure removed_callback) {
    TabStripLayoutHelper::TabSlot slot;
    slot.type = ViewType::kGroupHeader;
    slot.view = header;
    TabAnimationState initial_state = TabAnimationState::ForIdealTabState(
        TabAnimationState::TabOpenness::kOpen, pinned,
        TabAnimationState::TabActiveness::kInactive, 0);
    slot.animation = std::make_unique<TabAnimation>(
        initial_state, std::move(removed_callback));
    return slot;
  }

  ViewType type;
  TabSlotView* view;
  std::unique_ptr<TabAnimation> animation;
};

TabStripLayoutHelper::TabStripLayoutHelper(
    const TabStripController* controller,
    GetTabsCallback get_tabs_callback,
    GetGroupHeadersCallback get_group_headers_callback,
    base::RepeatingClosure on_animation_progressed)
    : controller_(controller),
      get_tabs_callback_(get_tabs_callback),
      get_group_headers_callback_(get_group_headers_callback),
      on_animation_progressed_(on_animation_progressed),
      active_tab_width_(TabStyle::GetStandardWidth()),
      inactive_tab_width_(TabStyle::GetStandardWidth()),
      first_non_pinned_tab_index_(0),
      first_non_pinned_tab_x_(0) {}

TabStripLayoutHelper::~TabStripLayoutHelper() = default;

std::vector<Tab*> TabStripLayoutHelper::GetTabs() {
  std::vector<Tab*> tabs;
  for (const TabSlot& slot : slots_) {
    if (slot.type == ViewType::kTab)
      tabs.push_back(static_cast<Tab*>(slot.view));
  }

  return tabs;
}

bool TabStripLayoutHelper::IsAnimating() const {
  return animation_timer_.IsRunning();
}

int TabStripLayoutHelper::GetPinnedTabCount() const {
  views::ViewModelT<Tab>* tabs = get_tabs_callback_.Run();
  int pinned_count = 0;
  while (pinned_count < tabs->view_size() &&
         tabs->view_at(pinned_count)->data().pinned) {
    pinned_count++;
  }
  return pinned_count;
}

void TabStripLayoutHelper::InsertTabAtNoAnimation(
    int model_index,
    Tab* tab,
    base::OnceClosure tab_removed_callback,
    TabAnimationState::TabActiveness active,
    TabAnimationState::TabPinnedness pinned) {
  const int slot_index = GetSlotIndexForTabModelIndex(model_index);
  slots_.insert(
      slots_.begin() + slot_index,
      TabSlot::CreateForTab(tab, TabAnimationState::TabOpenness::kOpen, pinned,
                            std::move(tab_removed_callback)));
}

void TabStripLayoutHelper::InsertTabAt(
    int model_index,
    Tab* tab,
    base::OnceClosure tab_removed_callback,
    TabAnimationState::TabActiveness active,
    TabAnimationState::TabPinnedness pinned) {
  const int slot_index = GetSlotIndexForTabModelIndex(model_index);
  slots_.insert(
      slots_.begin() + slot_index,
      TabSlot::CreateForTab(tab, TabAnimationState::TabOpenness::kClosed,
                            pinned, std::move(tab_removed_callback)));
  AnimateSlot(slot_index,
              slots_[slot_index].animation->target_state().WithOpenness(
                  TabAnimationState::TabOpenness::kOpen));
}

void TabStripLayoutHelper::RemoveTabNoAnimation(int model_index) {
  TabAnimation* animation =
      slots_[GetSlotIndexForTabModelIndex(model_index)].animation.get();
  animation->AnimateTo(animation->target_state().WithOpenness(
      TabAnimationState::TabOpenness::kClosed));
  animation->CompleteAnimation();
}

void TabStripLayoutHelper::RemoveTab(int model_index) {
  int slot_index = GetSlotIndexForTabModelIndex(model_index);
  AnimateSlot(slot_index,
              slots_[slot_index].animation->target_state().WithOpenness(
                  TabAnimationState::TabOpenness::kClosed));
}

void TabStripLayoutHelper::OnTabDestroyed(Tab* tab) {
  auto it =
      std::find_if(slots_.begin(), slots_.end(), [tab](const TabSlot& slot) {
        return slot.type == ViewType::kTab && slot.view == tab;
      });
  // Remove the tab from |slots_| if it is still there. It will have already
  // been removed if the tab was destroyed by |RemoveClosedTabs|.
  if (it != slots_.end())
    slots_.erase(it);
}

void TabStripLayoutHelper::MoveTab(
    base::Optional<TabGroupId> group_at_prev_index,
    int prev_index,
    int new_index) {
  const int prev_slot_index = GetSlotIndexForTabModelIndex(prev_index);
  TabSlot moving_tab = std::move(slots_[prev_slot_index]);
  slots_.erase(slots_.begin() + prev_slot_index);

  const int new_slot_index = GetSlotIndexForTabModelIndex(new_index);
  slots_.insert(slots_.begin() + new_slot_index, std::move(moving_tab));

  if (group_at_prev_index.has_value())
    UpdateGroupHeaderIndex(group_at_prev_index.value());
}

void TabStripLayoutHelper::SetTabPinnedness(
    int model_index,
    TabAnimationState::TabPinnedness pinnedness) {
  // TODO(958173): Animate state change.
  TabAnimation* animation =
      slots_[GetSlotIndexForTabModelIndex(model_index)].animation.get();
  animation->AnimateTo(animation->target_state().WithPinnedness(pinnedness));
  animation->CompleteAnimation();
}

void TabStripLayoutHelper::InsertGroupHeader(
    TabGroupId group,
    TabGroupHeader* header,
    base::OnceClosure header_removed_callback) {
  // TODO(958173): Animate open.
  std::vector<int> tabs_in_group = controller_->ListTabsInGroup(group);
  const int header_slot_index = GetSlotIndexForTabModelIndex(tabs_in_group[0]);
  slots_.insert(slots_.begin() + header_slot_index,
                TabSlot::CreateForGroupHeader(
                    group, header, TabAnimationState::TabPinnedness::kUnpinned,
                    std::move(header_removed_callback)));
}

void TabStripLayoutHelper::RemoveGroupHeader(TabGroupId group) {
  // TODO(958173): Animate closed.
  const int slot_index = GetSlotIndexForGroupHeader(group);
  slots_[slot_index].animation->NotifyCloseCompleted();
  slots_.erase(slots_.begin() + slot_index);
}

void TabStripLayoutHelper::UpdateGroupHeaderIndex(TabGroupId group) {
  const int slot_index = GetSlotIndexForGroupHeader(group);
  TabSlot header_slot = std::move(slots_[slot_index]);

  slots_.erase(slots_.begin() + slot_index);
  std::vector<int> tabs_in_group = controller_->ListTabsInGroup(group);
  const int first_tab_slot_index =
      GetSlotIndexForTabModelIndex(tabs_in_group[0]);
  slots_.insert(slots_.begin() + first_tab_slot_index, std::move(header_slot));
}

void TabStripLayoutHelper::SetActiveTab(int prev_active_index,
                                        int new_active_index) {
  // Set activeness without animating by retargeting the existing animation.
  if (prev_active_index >= 0) {
    const int prev_slot_index = GetSlotIndexForTabModelIndex(prev_active_index);
    TabAnimation* animation = slots_[prev_slot_index].animation.get();
    animation->RetargetTo(animation->target_state().WithActiveness(
        TabAnimationState::TabActiveness::kInactive));
  }
  if (new_active_index >= 0) {
    const int new_slot_index = GetSlotIndexForTabModelIndex(new_active_index);
    TabAnimation* animation = slots_[new_slot_index].animation.get();
    animation->RetargetTo(animation->target_state().WithActiveness(
        TabAnimationState::TabActiveness::kActive));
  }
}

void TabStripLayoutHelper::CompleteAnimations() {
  CompleteAnimationsWithoutDestroyingTabs();
  RemoveClosedTabs();
}

void TabStripLayoutHelper::CompleteAnimationsWithoutDestroyingTabs() {
  for (TabSlot& slot : slots_)
    slot.animation->CompleteAnimation();
  animation_timer_.Stop();
}

void TabStripLayoutHelper::UpdateIdealBounds(int available_width) {
  const int active_tab_model_index = controller_->GetActiveIndex();
  const int active_tab_slot_index =
      controller_->IsValidIndex(active_tab_model_index)
          ? GetSlotIndexForTabModelIndex(active_tab_model_index)
          : TabStripModel::kNoTab;
  const int pinned_tab_count = GetPinnedTabCount();
  const int last_pinned_tab_slot_index =
      pinned_tab_count > 0 ? GetSlotIndexForTabModelIndex(pinned_tab_count - 1)
                           : TabStripModel::kNoTab;

  TabLayoutConstants layout_constants = GetTabLayoutConstants();
  std::vector<TabWidthConstraints> tab_widths;
  for (int i = 0; i < int{slots_.size()}; i++) {
    auto active = i == active_tab_slot_index
                      ? TabAnimationState::TabActiveness::kActive
                      : TabAnimationState::TabActiveness::kInactive;
    auto pinned = i <= last_pinned_tab_slot_index
                      ? TabAnimationState::TabPinnedness::kPinned
                      : TabAnimationState::TabPinnedness::kUnpinned;
    auto open = slots_[i].animation->IsClosing()
                    ? TabAnimationState::TabOpenness::kClosed
                    : TabAnimationState::TabOpenness::kOpen;
    TabAnimationState ideal_animation_state =
        TabAnimationState::ForIdealTabState(open, pinned, active, 0);
    TabSizeInfo size_info = slots_[i].view->GetTabSizeInfo();
    tab_widths.push_back(TabWidthConstraints(ideal_animation_state,
                                             layout_constants, size_info));
  }

  const std::vector<gfx::Rect> bounds =
      CalculateTabBounds(layout_constants, tab_widths, available_width);
  DCHECK_EQ(slots_.size(), bounds.size());

  views::ViewModelT<Tab>* tabs = get_tabs_callback_.Run();
  std::map<TabGroupId, TabGroupHeader*> group_headers =
      get_group_headers_callback_.Run();

  int current_tab_model_index = 0;
  for (int i = 0; i < int{bounds.size()}; ++i) {
    const TabSlot& slot = slots_[i];
    switch (slot.type) {
      case ViewType::kTab:
        if (!slot.animation->IsClosing()) {
          tabs->set_ideal_bounds(current_tab_model_index, bounds[i]);
          UpdateCachedTabWidth(i, bounds[i].width(),
                               i == active_tab_slot_index);
          ++current_tab_model_index;
        }
        break;
      case ViewType::kGroupHeader:
        slot.view->SetBoundsRect(bounds[i]);
        break;
    }
  }
}

void TabStripLayoutHelper::UpdateIdealBoundsForPinnedTabs() {
  views::ViewModelT<Tab>* tabs = get_tabs_callback_.Run();
  const int pinned_tab_count = GetPinnedTabCount();

  first_non_pinned_tab_index_ = pinned_tab_count;
  first_non_pinned_tab_x_ = 0;

  TabLayoutConstants layout_constants = GetTabLayoutConstants();
  if (pinned_tab_count > 0) {
    std::vector<TabWidthConstraints> tab_widths;
    for (int tab_index = 0; tab_index < pinned_tab_count; tab_index++) {
      TabAnimationState ideal_animation_state =
          TabAnimationState::ForIdealTabState(
              TabAnimationState::TabOpenness::kOpen,
              TabAnimationState::TabPinnedness::kPinned,
              TabAnimationState::TabActiveness::kInactive, 0);
      TabSizeInfo size_info = tabs->view_at(tab_index)->GetTabSizeInfo();
      tab_widths.push_back(TabWidthConstraints(ideal_animation_state,
                                               layout_constants, size_info));
    }

    const std::vector<gfx::Rect> tab_bounds =
        CalculatePinnedTabBounds(layout_constants, tab_widths);

    for (int i = 0; i < pinned_tab_count; ++i)
      tabs->set_ideal_bounds(i, tab_bounds[i]);
  }
}

int TabStripLayoutHelper::LayoutTabs(int available_width) {
  std::vector<gfx::Rect> bounds =
      CalculateTabBounds(GetTabLayoutConstants(),
                         GetCurrentTabWidthConstraints(), available_width);

  if (DCHECK_IS_ON()) {
    views::ViewModelT<Tab>* tabs = get_tabs_callback_.Run();
    std::map<TabGroupId, TabGroupHeader*> group_headers =
        get_group_headers_callback_.Run();

    int num_closing_tabs = 0;
    for (Tab* tab : GetTabs()) {
      if (tab->closing())
        ++num_closing_tabs;
    }
    DCHECK_EQ(bounds.size(),
              tabs->view_size() + num_closing_tabs + group_headers.size());
  }

  int trailing_x = 0;

  const int active_tab_model_index = controller_->GetActiveIndex();
  int current_tab_model_index = 0;

  for (size_t i = 0; i < bounds.size(); i++) {
    switch (slots_[i].type) {
      case ViewType::kTab: {
        Tab* tab = static_cast<Tab*>(slots_[i].view);
        if (!tab->dragging()) {
          tab->SetBoundsRect(bounds[i]);
          trailing_x = std::max(trailing_x, bounds[i].right());
          // TODO(958173): We shouldn't need to update the cached widths here,
          // since they're also updated in UpdateIdealBounds. However, tests
          // will fail without this line; we should investigate why.
          UpdateCachedTabWidth(
              current_tab_model_index, bounds[i].width(),
              current_tab_model_index == active_tab_model_index);
        }
        if (!slots_[i].animation->IsClosing())
          ++current_tab_model_index;
        break;
      }
      case ViewType::kGroupHeader: {
        slots_[i].view->SetBoundsRect(bounds[i]);
        trailing_x = std::max(trailing_x, bounds[i].right());
        break;
      }
    }
  }

  return trailing_x;
}

int TabStripLayoutHelper::GetSlotIndexForTabModelIndex(int model_index) const {
  int current_model_index = 0;
  for (size_t i = 0; i < slots_.size(); i++) {
    if (slots_[i].type == ViewType::kTab && !slots_[i].animation->IsClosing()) {
      if (current_model_index == model_index)
        return i;
      ++current_model_index;
    }
  }
  DCHECK_EQ(model_index, current_model_index);
  return slots_.size();
}

int TabStripLayoutHelper::GetSlotIndexForGroupHeader(TabGroupId group) const {
  for (size_t i = 0; i < slots_.size(); i++) {
    if (slots_[i].type == ViewType::kGroupHeader &&
        static_cast<TabGroupHeader*>(slots_[i].view)->group() == group) {
      return i;
    }
  }
  NOTREACHED();
  return 0;
}

std::vector<TabWidthConstraints>
TabStripLayoutHelper::GetCurrentTabWidthConstraints() const {
  TabLayoutConstants layout_constants = GetTabLayoutConstants();
  std::vector<TabWidthConstraints> result;
  for (const TabSlot& slot : slots_) {
    result.push_back(slot.animation->GetCurrentTabWidthConstraints(
        layout_constants, slot.view->GetTabSizeInfo()));
  }
  return result;
}

void TabStripLayoutHelper::AnimateSlot(int slots_index,
                                       TabAnimationState target_state) {
  slots_[slots_index].animation->AnimateTo(target_state);
  if (!animation_timer_.IsRunning()) {
    animation_timer_.Start(FROM_HERE, kTickInterval, this,
                           &TabStripLayoutHelper::TickAnimations);
    // Tick animations immediately so that the animation starts from the
    // beginning instead of kTickInterval ms into the animation.
    TickAnimations();
  }
}

void TabStripLayoutHelper::TickAnimations() {
  bool all_animations_completed = true;
  for (const TabSlot& slot : slots_)
    all_animations_completed &= slot.animation->GetTimeRemaining().is_zero();
  RemoveClosedTabs();

  if (all_animations_completed)
    animation_timer_.Stop();

  on_animation_progressed_.Run();
}

void TabStripLayoutHelper::RemoveClosedTabs() {
  for (auto it = slots_.begin(); it != slots_.end();) {
    if (it->animation->IsClosed()) {
      // Remove the closed tab from |slots_| before invoking the callback so
      // that this is in a consistent state and can be reentered.
      TabSlot removed_slot = std::move(*it);
      it = slots_.erase(it);
      removed_slot.animation->NotifyCloseCompleted();
    } else {
      it++;
    }
  }
}

void TabStripLayoutHelper::UpdateCachedTabWidth(int tab_index,
                                                int tab_width,
                                                bool active) {
  if (active)
    active_tab_width_ = tab_width;
  else
    inactive_tab_width_ = tab_width;
}
