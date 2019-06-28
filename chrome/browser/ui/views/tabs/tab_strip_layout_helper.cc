// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_layout_helper.h"

#include <algorithm>
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

}  // namespace

struct TabStripLayoutHelper::TabSlot {
  static TabStripLayoutHelper::TabSlot CreateForTab(int tab_model_index,
                                                    bool pinned) {
    TabStripLayoutHelper::TabSlot slot;
    slot.type = TabAnimation::ViewType::kTab;
    slot.tab_model_index = tab_model_index;
    slot.pinned = pinned;
    slot.active = false;
    return slot;
  }

  static TabStripLayoutHelper::TabSlot CreateForGroupHeader(TabGroupId group,
                                                            bool pinned) {
    TabStripLayoutHelper::TabSlot slot;
    slot.type = TabAnimation::ViewType::kGroupHeader;
    slot.group = group;
    slot.pinned = pinned;
    slot.active = false;
    return slot;
  }

  int GetTabModelIndex() const {
    DCHECK(tab_model_index.has_value());
    return tab_model_index.value();
  }

  TabGroupId GetGroup() const {
    DCHECK(group.has_value());
    return group.value();
  }

  TabAnimation::ViewType type;
  base::Optional<int> tab_model_index;
  base::Optional<TabGroupId> group;
  bool pinned;
  bool active;
};

TabStripLayoutHelper::TabStripLayoutHelper(
    const TabStripController* controller,
    GetTabsCallback get_tabs_callback,
    GetGroupHeadersCallback get_group_headers_callback,
    base::RepeatingClosure on_animation_progressed)
    : controller_(controller),
      get_tabs_callback_(get_tabs_callback),
      get_group_headers_callback_(get_group_headers_callback),
      animator_(on_animation_progressed),
      active_tab_width_(TabStyle::GetStandardWidth()),
      inactive_tab_width_(TabStyle::GetStandardWidth()),
      first_non_pinned_tab_index_(0),
      first_non_pinned_tab_x_(0) {}

TabStripLayoutHelper::~TabStripLayoutHelper() = default;

bool TabStripLayoutHelper::IsAnimating() const {
  return animator_.IsAnimating();
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
    int index,
    base::OnceClosure tab_removed_callback,
    TabAnimationState::TabActiveness active,
    TabAnimationState::TabPinnedness pinned) {
  UpdateCachedTabSlots();
  animator_.InsertTabAtNoAnimation(
      TabAnimation::ViewType::kTab, AnimatorIndexForTab(index),
      std::move(tab_removed_callback), active, pinned);
  VerifyAnimationsMatchTabSlots();
}

void TabStripLayoutHelper::InsertTabAt(
    int index,
    base::OnceClosure tab_removed_callback,
    TabAnimationState::TabActiveness active,
    TabAnimationState::TabPinnedness pinned) {
  UpdateCachedTabSlots();
  animator_.InsertTabAt(TabAnimation::ViewType::kTab,
                        AnimatorIndexForTab(index),
                        std::move(tab_removed_callback), active, pinned);
  VerifyAnimationsMatchTabSlots();
}

void TabStripLayoutHelper::RemoveTabAt(int index) {
  // Remove animation before calling UpdateCachedTabSlots so that we can get the
  // correct animator index.
  // TODO(958173): Animate closed.
  animator_.RemoveTabNoAnimation(AnimatorIndexForTab(index));
  UpdateCachedTabSlots();
  VerifyAnimationsMatchTabSlots();
}

void TabStripLayoutHelper::MoveTab(
    base::Optional<TabGroupId> group_at_prev_index,
    int prev_index,
    int new_index) {
  int prev_header_animator_index;
  if (group_at_prev_index.has_value()) {
    prev_header_animator_index =
        AnimatorIndexForGroupHeader(group_at_prev_index.value());
  }
  const int prev_tab_animator_index = AnimatorIndexForTab(prev_index);

  UpdateCachedTabSlots();

  int target_animator_index = AnimatorIndexForTab(new_index);

  std::vector<int> moving_tabs;
  if (group_at_prev_index.has_value()) {
    const int new_header_animator_index =
        AnimatorIndexForGroupHeader(group_at_prev_index.value());
    if (prev_header_animator_index != new_header_animator_index) {
      moving_tabs.push_back(prev_header_animator_index);
      target_animator_index = new_header_animator_index;
    }
  }
  moving_tabs.push_back(prev_tab_animator_index);

  // TODO(958173): Animate move.
  animator_.MoveTabsNoAnimation(std::move(moving_tabs), target_animator_index);

  VerifyAnimationsMatchTabSlots();
}

void TabStripLayoutHelper::SetTabPinnedness(
    int index,
    TabAnimationState::TabPinnedness pinnedness) {
  UpdateCachedTabSlots();
  // TODO(958173): Animate state change.
  animator_.SetPinnednessNoAnimation(AnimatorIndexForTab(index), pinnedness);
  VerifyAnimationsMatchTabSlots();
}

void TabStripLayoutHelper::InsertGroupHeader(
    TabGroupId group,
    base::OnceClosure header_removed_callback) {
  UpdateCachedTabSlots();
  // TODO(958173): Animate open.
  animator_.InsertTabAtNoAnimation(TabAnimation::ViewType::kGroupHeader,
                                   AnimatorIndexForGroupHeader(group),
                                   std::move(header_removed_callback),
                                   TabAnimationState::TabActiveness::kInactive,
                                   TabAnimationState::TabPinnedness::kUnpinned);
  VerifyAnimationsMatchTabSlots();
}

void TabStripLayoutHelper::RemoveGroupHeader(TabGroupId group) {
  // Remove animation before calling UpdateCachedTabSlots so that we can get the
  // correct animator index.
  // TODO(958173): Animate closed.
  animator_.RemoveTabNoAnimation(AnimatorIndexForGroupHeader(group));
  UpdateCachedTabSlots();
  VerifyAnimationsMatchTabSlots();
}

void TabStripLayoutHelper::SetActiveTab(int prev_active_index,
                                        int new_active_index) {
  UpdateCachedTabSlots();
  animator_.SetActiveTab(AnimatorIndexForTab(prev_active_index),
                         AnimatorIndexForTab(new_active_index));
  VerifyAnimationsMatchTabSlots();
}

void TabStripLayoutHelper::CompleteAnimations() {
  animator_.CompleteAnimations();
}

void TabStripLayoutHelper::CompleteAnimationsWithoutDestroyingTabs() {
  animator_.CompleteAnimationsWithoutDestroyingTabs();
}

void TabStripLayoutHelper::UpdateIdealBounds(int available_width) {
  std::vector<TabAnimationState> ideal_animation_states;
  for (const auto& slot : cached_slots_) {
    auto pinned = slot.pinned ? TabAnimationState::TabPinnedness::kPinned
                              : TabAnimationState::TabPinnedness::kUnpinned;
    auto active = slot.active ? TabAnimationState::TabActiveness::kActive
                              : TabAnimationState::TabActiveness::kInactive;
    ideal_animation_states.push_back(TabAnimationState::ForIdealTabState(
        TabAnimationState::TabOpenness::kOpen, pinned, active, 0));
  }

  const std::vector<gfx::Rect> bounds = CalculateTabBounds(
      GetTabSizeInfo(), ideal_animation_states, available_width);
  DCHECK_EQ(cached_slots_.size(), bounds.size());

  views::ViewModelT<Tab>* tabs = get_tabs_callback_.Run();
  std::map<TabGroupId, TabGroupHeader*> group_headers =
      get_group_headers_callback_.Run();

  for (size_t i = 0; i < bounds.size(); ++i) {
    const TabSlot& slot = cached_slots_[i];
    switch (slot.type) {
      case TabAnimation::ViewType::kTab:
        tabs->set_ideal_bounds(slot.GetTabModelIndex(), bounds[i]);
        UpdateCachedTabWidth(i, bounds[i].width(), slot.active);
        break;
      case TabAnimation::ViewType::kGroupHeader:
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
      GetTabSizeInfo(), animator_.GetCurrentTabStates(), available_width);

  // TODO(958173): Assume for now that there are no closing tabs or headers.
  DCHECK_EQ(bounds.size(), tabs->view_size() + group_headers.size());

  int trailing_x = 0;

  const int active_tab_model_index = controller_->GetActiveIndex();
  for (int i = 0; i < tabs->view_size(); i++) {
    if (tabs->view_at(i)->dragging())
      continue;
    const int animator_index = AnimatorIndexForTab(i);
    tabs->view_at(i)->SetBoundsRect(bounds[animator_index]);
    trailing_x = std::max(trailing_x, bounds[animator_index].right());
    // TODO(958173): We shouldn't need to update the cached widths here, since
    // they're also updated in UpdateIdealBounds. However, tests will fail
    // without this line; we should investigate why.
    UpdateCachedTabWidth(i, bounds[animator_index].width(),
                         i == active_tab_model_index);
  }

  for (const auto& header_pair : group_headers) {
    const int animator_index = AnimatorIndexForGroupHeader(header_pair.first);
    header_pair.second->SetBoundsRect(bounds[animator_index]);
    trailing_x = std::max(trailing_x, bounds[animator_index].right());
  }

  return trailing_x;
}

void TabStripLayoutHelper::UpdateCachedTabSlots() {
  views::ViewModelT<Tab>* tabs = get_tabs_callback_.Run();
  std::map<TabGroupId, TabGroupHeader*> group_headers =
      get_group_headers_callback_.Run();

  std::vector<base::Optional<TabGroupId>> tab_to_group_mapping(
      tabs->view_size());
  for (const auto& header_pair : group_headers) {
    const TabGroupId group = header_pair.first;
    std::vector<int> tabs = controller_->ListTabsInGroup(group);
    for (int tab : tabs)
      tab_to_group_mapping[tab] = group;
  }

  const int num_pinned_tabs = GetPinnedTabCount();
  const int active_tab_index = controller_->GetActiveIndex();

  std::vector<TabSlot> new_slots;
  std::set<TabGroupId> headers_already_added;
  for (int i = 0; i < tabs->view_size(); ++i) {
    const bool pinned = i < num_pinned_tabs;
    base::Optional<TabGroupId> group = tab_to_group_mapping[i];
    if (group.has_value() &&
        !base::Contains(headers_already_added, group.value())) {
      // Start of a group.
      new_slots.push_back(TabSlot::CreateForGroupHeader(group.value(), pinned));
      headers_already_added.insert(group.value());
    }
    TabSlot slot = TabSlot::CreateForTab(i, pinned);
    if (i == active_tab_index)
      slot.active = true;
    new_slots.push_back(slot);
  }

  cached_slots_ = std::move(new_slots);
}

int TabStripLayoutHelper::AnimatorIndexForTab(int tab_model_index) const {
  if (tab_model_index == TabStripModel::kNoTab)
    return TabStripModel::kNoTab;

  auto it = std::find_if(cached_slots_.begin(), cached_slots_.end(),
                         [&tab_model_index](const TabSlot& tab_slot) {
                           return tab_slot.tab_model_index == tab_model_index;
                         });
  DCHECK(it != cached_slots_.end());
  return it - cached_slots_.begin();
}

int TabStripLayoutHelper::AnimatorIndexForGroupHeader(TabGroupId group) const {
  auto it = std::find_if(
      cached_slots_.begin(), cached_slots_.end(),
      [&group](const TabSlot& tab_slot) { return tab_slot.group == group; });
  DCHECK(it != cached_slots_.end());
  return it - cached_slots_.begin();
}

void TabStripLayoutHelper::VerifyAnimationsMatchTabSlots() const {
  if (!DCHECK_IS_ON())
    return;
  DCHECK_EQ(cached_slots_.size(), animator_.animation_count());
  for (size_t i = 0; i < cached_slots_.size(); i++) {
    DCHECK_EQ(cached_slots_[i].type, animator_.GetAnimationViewTypeAt(i))
        << "At index " << i;
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
