// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_layout_helper.h"

#include <set>

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/tab.h"
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

TabStripLayoutHelper::TabStripLayoutHelper()
    : active_tab_width_(TabStyle::GetStandardWidth()),
      inactive_tab_width_(TabStyle::GetStandardWidth()),
      first_non_pinned_tab_index_(0),
      first_non_pinned_tab_x_(0) {}

TabStripLayoutHelper::~TabStripLayoutHelper() = default;

int TabStripLayoutHelper::GetPinnedTabCount(
    const views::ViewModelT<Tab>* tabs) const {
  int pinned_count = 0;
  while (pinned_count < tabs->view_size() &&
         tabs->view_at(pinned_count)->data().pinned) {
    pinned_count++;
  }
  return pinned_count;
}

namespace {

// Helper types for UpdateIdealBounds.

enum class TabSlotType {
  kTab,
  kGroupHeader,
};

struct TabSlot {
  static TabSlot CreateForTab(int tab, bool pinned) {
    TabSlot slot;
    slot.type = TabSlotType::kTab;
    slot.tab = tab;
    slot.pinned = pinned;
    return slot;
  }

  static TabSlot CreateForGroupHeader(TabGroupId group, bool pinned) {
    TabSlot slot;
    slot.type = TabSlotType::kGroupHeader;
    slot.group = group;
    slot.pinned = pinned;
    return slot;
  }

  int GetTab() const {
    DCHECK(tab.has_value());
    return tab.value();
  }

  TabGroupId GetGroup() const {
    DCHECK(group.has_value());
    return group.value();
  }

  TabSlotType type;
  base::Optional<int> tab;
  base::Optional<TabGroupId> group;
  bool pinned;
};

}  // namespace

void TabStripLayoutHelper::UpdateIdealBounds(
    TabStripController* controller,
    views::ViewModelT<Tab>* tabs,
    std::map<TabGroupId, TabGroupHeader*> group_headers,
    int available_width) {
  std::vector<base::Optional<TabGroupId>> tab_to_group_mapping(
      tabs->view_size());
  for (const auto& header_pair : group_headers) {
    const TabGroupId group = header_pair.first;
    std::vector<int> tabs = controller->ListTabsInGroup(group);
    for (int tab : tabs)
      tab_to_group_mapping[tab] = group;
  }

  const int num_pinned_tabs = GetPinnedTabCount(tabs);
  const int active_tab_index = controller->GetActiveIndex();

  std::vector<TabSlot> slots;
  std::set<TabGroupId> headers_already_added;
  int active_index_in_slots = TabStripModel::kNoTab;
  for (int i = 0; i < tabs->view_size(); ++i) {
    const bool pinned = i < num_pinned_tabs;
    base::Optional<TabGroupId> group = tab_to_group_mapping[i];
    if (group.has_value() &&
        !base::Contains(headers_already_added, group.value())) {
      // Start of a group.
      slots.push_back(TabSlot::CreateForGroupHeader(group.value(), pinned));
      headers_already_added.insert(group.value());
    }
    slots.push_back(TabSlot::CreateForTab(i, pinned));
    if (i == active_tab_index)
      active_index_in_slots = slots.size() - 1;
  }

  std::vector<TabAnimationState> ideal_animation_states;
  for (int i = 0; i < int{slots.size()}; ++i) {
    ideal_animation_states.push_back(TabAnimationState::ForIdealTabState(
        TabAnimationState::TabOpenness::kOpen,
        slots[i].pinned ? TabAnimationState::TabPinnedness::kPinned
                        : TabAnimationState::TabPinnedness::kUnpinned,
        i == active_index_in_slots
            ? TabAnimationState::TabActiveness::kActive
            : TabAnimationState::TabActiveness::kInactive,
        0));
  }

  const std::vector<gfx::Rect> bounds = CalculateTabBounds(
      GetTabSizeInfo(), ideal_animation_states, available_width);
  DCHECK_EQ(slots.size(), bounds.size());

  for (size_t i = 0; i < bounds.size(); ++i) {
    const TabSlot& slot = slots[i];
    switch (slot.type) {
      case TabSlotType::kTab:
        tabs->set_ideal_bounds(slot.GetTab(), bounds[i]);
        UpdateCachedTabWidth(i, bounds[i].width(), active_index_in_slots);
        break;
      case TabSlotType::kGroupHeader:
        group_headers[slot.GetGroup()]->SetBoundsRect(bounds[i]);
        break;
    }
  }
}

void TabStripLayoutHelper::UpdateIdealBoundsForPinnedTabs(
    views::ViewModelT<Tab>* tabs) {
  const int pinned_tab_count = GetPinnedTabCount(tabs);

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

int TabStripLayoutHelper::LayoutTabs(views::ViewModelT<Tab>* tabs,
                                     std::vector<TabAnimationState> tab_states,
                                     int available_width,
                                     int active_tab_model_index) {
  std::vector<gfx::Rect> bounds =
      CalculateTabBounds(GetTabSizeInfo(), tab_states, available_width);

  // TODO(958173): Assume for now that there are no closing tabs.
  // TODO(958173): Assume for now that there are no group headers.
  DCHECK_EQ(bounds.size(), static_cast<size_t>(tabs->view_size()));
  int trailing_x = 0;
  for (size_t i = 0; i < bounds.size(); i++) {
    if (tabs->view_at(i)->dragging())
      continue;
    tabs->view_at(i)->SetBoundsRect(bounds[i]);
    trailing_x = std::max(trailing_x, bounds[i].right());
    // TODO(958173): We shouldn't need to update the cached widths here, since
    // they're also updated in UpdateIdealBounds. However, tests will fail
    // without this line; we should investigate why.
    UpdateCachedTabWidth(i, bounds[i].width(), active_tab_model_index);
  }

  return trailing_x;
}

void TabStripLayoutHelper::UpdateCachedTabWidth(int tab_index,
                                                int tab_width,
                                                int active_tab_index) {
  if (tab_index == active_tab_index)
    active_tab_width_ = tab_width;
  else
    inactive_tab_width_ = tab_width;
}
