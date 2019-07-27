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
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_layout.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/views/view_model.h"

namespace {

// TODO(965227): Align animation ticks to compositor events.
constexpr base::TimeDelta kTickInterval =
    base::TimeDelta::FromMilliseconds(1000 / 60.0);

const TabSizeInfo& GetTabSizeInfo() {
  static TabSizeInfo tab_size_info, touch_tab_size_info;
  TabSizeInfo* info = ui::MaterialDesignController::touch_ui()
                          ? &touch_tab_size_info
                          : &tab_size_info;
  if (info->standard_size.IsEmpty()) {
    info->pinned_tab_width = TabStyle::GetPinnedWidth();
    info->min_active_width = TabStyleViews::GetMinimumActiveWidth();
    info->min_inactive_width = TabStyleViews::GetMinimumInactiveWidth();
    info->standard_size =
        gfx::Size(TabStyle::GetStandardWidth(), GetLayoutConstant(TAB_HEIGHT));
    info->tab_overlap = TabStyle::GetTabOverlap();
  }
  return *info;
}

// The types of Views that can be represented by TabSlot.
enum class ViewType {
  kTab,
  kGroupHeader,
};

}  // namespace

struct TabStripLayoutHelper::TabSlot {
  static TabStripLayoutHelper::TabSlot CreateForTab(
      TabAnimationState::TabOpenness openness,
      TabAnimationState::TabPinnedness pinned,
      base::OnceClosure removed_callback) {
    TabStripLayoutHelper::TabSlot slot;
    slot.type = ViewType::kTab;
    TabAnimationState initial_state = TabAnimationState::ForIdealTabState(
        openness, pinned, TabAnimationState::TabActiveness::kInactive, 0);
    slot.animation = std::make_unique<TabAnimation>(
        initial_state, std::move(removed_callback));
    return slot;
  }

  static TabStripLayoutHelper::TabSlot CreateForGroupHeader(
      TabGroupId group,
      TabAnimationState::TabPinnedness pinned,
      base::OnceClosure removed_callback) {
    TabStripLayoutHelper::TabSlot slot;
    slot.type = ViewType::kGroupHeader;
    TabAnimationState initial_state = TabAnimationState::ForIdealTabState(
        TabAnimationState::TabOpenness::kOpen, pinned,
        TabAnimationState::TabActiveness::kInactive, 0);
    slot.animation = std::make_unique<TabAnimation>(
        initial_state, std::move(removed_callback));
    slot.group = group;
    return slot;
  }

  TabGroupId GetGroup() const {
    DCHECK(group.has_value());
    return group.value();
  }

  ViewType type;
  std::unique_ptr<TabAnimation> animation;
  base::Optional<TabGroupId> group;
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
    base::OnceClosure tab_removed_callback,
    TabAnimationState::TabActiveness active,
    TabAnimationState::TabPinnedness pinned) {
  const int slot_index = GetSlotIndexForTabModelIndex(model_index);
  slots_.insert(slots_.begin() + slot_index,
                TabSlot::CreateForTab(TabAnimationState::TabOpenness::kOpen,
                                      pinned, std::move(tab_removed_callback)));
}

void TabStripLayoutHelper::InsertTabAt(
    int model_index,
    base::OnceClosure tab_removed_callback,
    TabAnimationState::TabActiveness active,
    TabAnimationState::TabPinnedness pinned) {
  const int slot_index = GetSlotIndexForTabModelIndex(model_index);
  slots_.insert(slots_.begin() + slot_index,
                TabSlot::CreateForTab(TabAnimationState::TabOpenness::kClosed,
                                      pinned, std::move(tab_removed_callback)));
  AnimateSlot(slot_index,
              slots_[slot_index].animation->target_state().WithOpenness(
                  TabAnimationState::TabOpenness::kOpen));
}

void TabStripLayoutHelper::RemoveTabAt(int model_index) {
  // TODO(958173): Animate closed.
  slots_.erase(slots_.begin() + GetSlotIndexForTabModelIndex(model_index));
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

  if (group_at_prev_index.has_value()) {
    const int prev_group_slot_index =
        GetSlotIndexForGroupHeader(group_at_prev_index.value());
    TabSlot moving_group_header = std::move(slots_[prev_group_slot_index]);

    slots_.erase(slots_.begin() + prev_group_slot_index);
    std::vector<int> tabs_in_group =
        controller_->ListTabsInGroup(group_at_prev_index.value());
    const int first_tab_slot_index =
        GetSlotIndexForTabModelIndex(tabs_in_group[0]);
    slots_.insert(slots_.begin() + first_tab_slot_index,
                  std::move(moving_group_header));
  }
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
    base::OnceClosure header_removed_callback) {
  // TODO(958173): Animate open.
  std::vector<int> tabs_in_group = controller_->ListTabsInGroup(group);
  const int first_tab_slot_index =
      GetSlotIndexForTabModelIndex(tabs_in_group[0]);
  slots_.insert(slots_.begin() + first_tab_slot_index,
                TabSlot::CreateForGroupHeader(
                    group, TabAnimationState::TabPinnedness::kUnpinned,
                    std::move(header_removed_callback)));
}

void TabStripLayoutHelper::RemoveGroupHeader(TabGroupId group) {
  // TODO(958173): Animate closed.
  const int slot_index = GetSlotIndexForGroupHeader(group);
  slots_[slot_index].animation->NotifyCloseCompleted();
  slots_.erase(slots_.begin() + slot_index);
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

  std::vector<TabAnimationState> ideal_animation_states;
  for (int i = 0; i < int{slots_.size()}; i++) {
    auto active = i == active_tab_slot_index
                      ? TabAnimationState::TabActiveness::kActive
                      : TabAnimationState::TabActiveness::kInactive;
    auto pinned = i <= last_pinned_tab_slot_index
                      ? TabAnimationState::TabPinnedness::kPinned
                      : TabAnimationState::TabPinnedness::kUnpinned;
    ideal_animation_states.push_back(TabAnimationState::ForIdealTabState(
        TabAnimationState::TabOpenness::kOpen, pinned, active, 0));
  }

  const std::vector<gfx::Rect> bounds = CalculateTabBounds(
      GetTabSizeInfo(), ideal_animation_states, available_width);
  DCHECK_EQ(slots_.size(), bounds.size());

  views::ViewModelT<Tab>* tabs = get_tabs_callback_.Run();
  std::map<TabGroupId, TabGroupHeader*> group_headers =
      get_group_headers_callback_.Run();

  int current_tab_model_index = 0;
  for (int i = 0; i < int{bounds.size()}; ++i) {
    const TabSlot& slot = slots_[i];
    switch (slot.type) {
      case ViewType::kTab:
        tabs->set_ideal_bounds(current_tab_model_index, bounds[i]);
        UpdateCachedTabWidth(i, bounds[i].width(), i == active_tab_slot_index);
        ++current_tab_model_index;
        break;
      case ViewType::kGroupHeader:
        group_headers[slot.GetGroup()]->SetBoundsRect(bounds[i]);
        break;
    }
  }
}

void TabStripLayoutHelper::UpdateIdealBoundsForPinnedTabs() {
  views::ViewModelT<Tab>* tabs = get_tabs_callback_.Run();
  const int pinned_tab_count = GetPinnedTabCount();

  first_non_pinned_tab_index_ = pinned_tab_count;
  first_non_pinned_tab_x_ = 0;

  if (pinned_tab_count > 0) {
    std::vector<TabAnimationState> ideal_animation_states;
    for (int tab_index = 0; tab_index < pinned_tab_count; tab_index++) {
      ideal_animation_states.push_back(TabAnimationState::ForIdealTabState(
          TabAnimationState::TabOpenness::kOpen,
          TabAnimationState::TabPinnedness::kPinned,
          TabAnimationState::TabActiveness::kInactive, 0));
    }

    const std::vector<gfx::Rect> tab_bounds =
        CalculatePinnedTabBounds(GetTabSizeInfo(), ideal_animation_states);

    for (int i = 0; i < pinned_tab_count; ++i)
      tabs->set_ideal_bounds(i, tab_bounds[i]);
  }
}

int TabStripLayoutHelper::LayoutTabs(int available_width) {
  views::ViewModelT<Tab>* tabs = get_tabs_callback_.Run();
  std::map<TabGroupId, TabGroupHeader*> group_headers =
      get_group_headers_callback_.Run();

  std::vector<gfx::Rect> bounds = CalculateTabBounds(
      GetTabSizeInfo(), GetCurrentTabStates(), available_width);

  // TODO(958173): Assume for now that there are no closing tabs or headers.
  DCHECK_EQ(bounds.size(), tabs->view_size() + group_headers.size());

  int trailing_x = 0;

  const int active_tab_model_index = controller_->GetActiveIndex();
  int current_tab_model_index = 0;

  for (size_t i = 0; i < bounds.size(); i++) {
    switch (slots_[i].type) {
      case ViewType::kTab: {
        if (!tabs->view_at(current_tab_model_index)->dragging()) {
          tabs->view_at(current_tab_model_index)->SetBoundsRect(bounds[i]);
          trailing_x = std::max(trailing_x, bounds[i].right());
          // TODO(958173): We shouldn't need to update the cached widths here,
          // since they're also updated in UpdateIdealBounds. However, tests
          // will fail without this line; we should investigate why.
          UpdateCachedTabWidth(
              current_tab_model_index, bounds[i].width(),
              current_tab_model_index == active_tab_model_index);
        }
        ++current_tab_model_index;
        break;
      }
      case ViewType::kGroupHeader: {
        TabGroupHeader* header = group_headers[slots_[i].GetGroup()];
        header->SetBoundsRect(bounds[i]);
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
    if (slots_[i].type == ViewType::kTab) {
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
        slots_[i].GetGroup() == group) {
      return i;
    }
  }
  NOTREACHED();
  return 0;
}

std::vector<TabAnimationState> TabStripLayoutHelper::GetCurrentTabStates()
    const {
  std::vector<TabAnimationState> result;
  for (const TabSlot& slot : slots_)
    result.push_back(slot.animation->GetCurrentState());
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
    if (it->animation->GetTimeRemaining().is_zero() &&
        it->animation->GetCurrentState().IsFullyClosed()) {
      it->animation->NotifyCloseCompleted();
      it = slots_.erase(it);
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
