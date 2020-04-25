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
#include "chrome/browser/ui/tabs/tab_types.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_animation.h"
#include "chrome/browser/ui/views/tabs/tab_animation_state.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_slot_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_layout_types.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "ui/views/view_model.h"

namespace {

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
  static TabStripLayoutHelper::TabSlot CreateForTab(Tab* tab,
                                                    TabOpen open,
                                                    TabPinned pinned) {
    TabStripLayoutHelper::TabSlot slot;
    slot.type = ViewType::kTab;
    slot.view = tab;
    TabAnimationState initial_state = TabAnimationState::ForIdealTabState(
        open, pinned, TabActive::kInactive, 0);
    slot.animation = std::make_unique<TabAnimation>(initial_state);
    return slot;
  }

  static TabStripLayoutHelper::TabSlot CreateForGroupHeader(
      tab_groups::TabGroupId group,
      TabGroupHeader* header,
      TabPinned pinned) {
    TabStripLayoutHelper::TabSlot slot;
    slot.type = ViewType::kGroupHeader;
    slot.view = header;
    TabAnimationState initial_state = TabAnimationState::ForIdealTabState(
        TabOpen::kOpen, pinned, TabActive::kInactive, 0);
    slot.animation = std::make_unique<TabAnimation>(initial_state);
    return slot;
  }

  ViewType type;
  TabSlotView* view;
  std::unique_ptr<TabAnimation> animation;
};

TabStripLayoutHelper::TabStripLayoutHelper(
    const TabStripController* controller,
    GetTabsCallback get_tabs_callback,
    GetGroupHeadersCallback get_group_headers_callback)
    : controller_(controller),
      get_tabs_callback_(get_tabs_callback),
      get_group_headers_callback_(get_group_headers_callback),
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

int TabStripLayoutHelper::GetPinnedTabCount() const {
  views::ViewModelT<Tab>* tabs = get_tabs_callback_.Run();
  int pinned_count = 0;
  while (pinned_count < tabs->view_size() &&
         tabs->view_at(pinned_count)->data().pinned) {
    pinned_count++;
  }
  return pinned_count;
}

void TabStripLayoutHelper::InsertTabAt(int model_index,
                                       Tab* tab,
                                       TabPinned pinned) {
  const int slot_index =
      GetSlotIndexForTabModelIndex(model_index, tab->group());
  slots_.insert(slots_.begin() + slot_index,
                TabSlot::CreateForTab(tab, TabOpen::kOpen, pinned));
}

void TabStripLayoutHelper::RemoveTabAt(int model_index, Tab* tab) {
  TabAnimation* animation =
      slots_[GetSlotIndexForTabModelIndex(model_index, tab->group())]
          .animation.get();
  animation->AnimateTo(animation->target_state().WithOpen(TabOpen::kClosed));
  animation->CompleteAnimation();
}

void TabStripLayoutHelper::EnterTabClosingMode(int available_width) {
  if (!WidthsConstrainedForClosingMode()) {
    tab_width_override_ = CalculateTabWidthOverride(
        GetTabLayoutConstants(), GetCurrentTabWidthConstraints(),
        available_width);
    tabstrip_width_override_ = available_width;
  }
}

base::Optional<int> TabStripLayoutHelper::ExitTabClosingMode() {
  if (!WidthsConstrainedForClosingMode())
    return base::nullopt;

  int available_width = CalculateIdealBounds(base::nullopt).back().right();
  tab_width_override_.reset();
  tabstrip_width_override_.reset();

  return available_width;
}

void TabStripLayoutHelper::OnTabDestroyed(Tab* tab) {
  auto it =
      std::find_if(slots_.begin(), slots_.end(), [tab](const TabSlot& slot) {
        return slot.type == ViewType::kTab && slot.view == tab;
      });
  if (it != slots_.end())
    slots_.erase(it);
}

void TabStripLayoutHelper::MoveTab(
    base::Optional<tab_groups::TabGroupId> moving_tab_group,
    int prev_index,
    int new_index) {
  const int prev_slot_index =
      GetSlotIndexForTabModelIndex(prev_index, moving_tab_group);
  TabSlot moving_tab = std::move(slots_[prev_slot_index]);
  slots_.erase(slots_.begin() + prev_slot_index);

  const int new_slot_index =
      GetSlotIndexForTabModelIndex(new_index, moving_tab_group);
  slots_.insert(slots_.begin() + new_slot_index, std::move(moving_tab));

  if (moving_tab_group.has_value())
    UpdateGroupHeaderIndex(moving_tab_group.value());
}

void TabStripLayoutHelper::SetTabPinned(int model_index, TabPinned pinned) {
  views::ViewModelT<Tab>* tabs = get_tabs_callback_.Run();
  TabAnimation* animation =
      slots_[GetSlotIndexForTabModelIndex(model_index,
                                          tabs->view_at(model_index)->group())]
          .animation.get();
  animation->AnimateTo(animation->target_state().WithPinned(pinned));
  animation->CompleteAnimation();
}

void TabStripLayoutHelper::InsertGroupHeader(tab_groups::TabGroupId group,
                                             TabGroupHeader* header) {
  std::vector<int> tabs_in_group = controller_->ListTabsInGroup(group);
  const int header_slot_index =
      GetSlotIndexForTabModelIndex(tabs_in_group[0], group);
  slots_.insert(
      slots_.begin() + header_slot_index,
      TabSlot::CreateForGroupHeader(group, header, TabPinned::kUnpinned));

  // Set the starting location of the header to something reasonable for the
  // animation.
  slots_[header_slot_index].view->SetBoundsRect(
      GetTabs()[tabs_in_group[0]]->bounds());
}

void TabStripLayoutHelper::RemoveGroupHeader(tab_groups::TabGroupId group) {
  const int slot_index = GetSlotIndexForGroupHeader(group);
  slots_.erase(slots_.begin() + slot_index);
}

void TabStripLayoutHelper::UpdateGroupHeaderIndex(
    tab_groups::TabGroupId group) {
  const int slot_index = GetSlotIndexForGroupHeader(group);
  TabSlot header_slot = std::move(slots_[slot_index]);

  slots_.erase(slots_.begin() + slot_index);
  std::vector<int> tabs_in_group = controller_->ListTabsInGroup(group);
  const int first_tab_slot_index =
      GetSlotIndexForTabModelIndex(tabs_in_group[0], group);
  slots_.insert(slots_.begin() + first_tab_slot_index, std::move(header_slot));
}

void TabStripLayoutHelper::SetActiveTab(int prev_active_index,
                                        int new_active_index) {
  views::ViewModelT<Tab>* tabs = get_tabs_callback_.Run();
  // Set active state without animating by retargeting the existing animation.
  if (prev_active_index >= 0) {
    const int prev_slot_index = GetSlotIndexForTabModelIndex(
        prev_active_index, tabs->view_at(prev_active_index)->group());
    TabAnimation* animation = slots_[prev_slot_index].animation.get();
    animation->RetargetTo(
        animation->target_state().WithActive(TabActive::kInactive));
  }
  if (new_active_index >= 0) {
    const int new_slot_index = GetSlotIndexForTabModelIndex(
        new_active_index, tabs->view_at(new_active_index)->group());
    TabAnimation* animation = slots_[new_slot_index].animation.get();
    animation->RetargetTo(
        animation->target_state().WithActive(TabActive::kActive));
  }
}

int TabStripLayoutHelper::CalculateMinimumWidth() {
  const std::vector<gfx::Rect> bounds = CalculateIdealBounds(0);

  return bounds.empty() ? 0 : bounds.back().right();
}

int TabStripLayoutHelper::CalculatePreferredWidth() {
  const std::vector<gfx::Rect> bounds = CalculateIdealBounds(base::nullopt);

  return bounds.empty() ? 0 : bounds.back().right();
}

int TabStripLayoutHelper::UpdateIdealBounds(int available_width) {
  const std::vector<gfx::Rect> bounds = CalculateIdealBounds(available_width);
  DCHECK_EQ(slots_.size(), bounds.size());

  views::ViewModelT<Tab>* tabs = get_tabs_callback_.Run();
  const int active_tab_model_index = controller_->GetActiveIndex();
  const int active_tab_slot_index =
      controller_->IsValidIndex(active_tab_model_index)
          ? GetSlotIndexForTabModelIndex(
                active_tab_model_index,
                tabs->view_at(active_tab_model_index)->group())
          : TabStripModel::kNoTab;

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
        if (slot.view->dragging()) {
          slot.view->SetBoundsRect(bounds[i]);
        } else {
          group_header_ideal_bounds_[slot.view->group().value()] = bounds[i];
        }
        break;
    }
  }

  return bounds.back().right();
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
              TabOpen::kOpen, TabPinned::kPinned, TabActive::kInactive, 0);
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

std::vector<gfx::Rect> TabStripLayoutHelper::CalculateIdealBounds(
    base::Optional<int> available_width) {
  base::Optional<int> tabstrip_width = tabstrip_width_override_.has_value()
                                           ? tabstrip_width_override_
                                           : available_width;

  views::ViewModelT<Tab>* tabs = get_tabs_callback_.Run();
  std::map<tab_groups::TabGroupId, TabGroupHeader*> group_headers =
      get_group_headers_callback_.Run();

  const int active_tab_model_index = controller_->GetActiveIndex();
  const int active_tab_slot_index =
      controller_->IsValidIndex(active_tab_model_index)
          ? GetSlotIndexForTabModelIndex(
                active_tab_model_index,
                tabs->view_at(active_tab_model_index)->group())
          : TabStripModel::kNoTab;
  const int pinned_tab_count = GetPinnedTabCount();
  const int last_pinned_tab_index = pinned_tab_count - 1;
  const int last_pinned_tab_slot_index =
      pinned_tab_count > 0 ? GetSlotIndexForTabModelIndex(
                                 last_pinned_tab_index,
                                 tabs->view_at(last_pinned_tab_index)->group())
                           : TabStripModel::kNoTab;

  TabLayoutConstants layout_constants = GetTabLayoutConstants();
  std::vector<TabWidthConstraints> tab_widths;
  for (int i = 0; i < int{slots_.size()}; i++) {
    auto active =
        i == active_tab_slot_index ? TabActive::kActive : TabActive::kInactive;
    auto pinned = i <= last_pinned_tab_slot_index ? TabPinned::kPinned
                                                  : TabPinned::kUnpinned;
    auto open =
        slots_[i].animation->IsClosing() ? TabOpen::kClosed : TabOpen::kOpen;
    TabAnimationState ideal_animation_state =
        TabAnimationState::ForIdealTabState(open, pinned, active, 0);
    TabSizeInfo size_info = slots_[i].view->GetTabSizeInfo();
    tab_widths.push_back(TabWidthConstraints(ideal_animation_state,
                                             layout_constants, size_info));
  }

  return CalculateTabBounds(layout_constants, tab_widths, tabstrip_width,
                            tab_width_override_);
}

int TabStripLayoutHelper::GetSlotIndexForTabModelIndex(
    int model_index,
    base::Optional<tab_groups::TabGroupId> group) const {
  int current_model_index = 0;
  for (size_t i = 0; i < slots_.size(); i++) {
    const bool model_space_index =
        slots_[i].type == ViewType::kTab && !slots_[i].animation->IsClosing();
    if (current_model_index == model_index) {
      if (model_space_index) {
        return i;
      } else if (slots_[i].type == ViewType::kGroupHeader) {
        // If the tab is in the header's group, then it should be to its right,
        // so as to be contiguous with the group. If not, it goes to its left.
        return static_cast<TabGroupHeader*>(slots_[i].view)->group() == group
                   ? i + 1
                   : i;
      }
    }
    if (model_space_index)
      ++current_model_index;
  }
  DCHECK_EQ(model_index, current_model_index);
  return slots_.size();
}

int TabStripLayoutHelper::GetSlotIndexForGroupHeader(
    tab_groups::TabGroupId group) const {
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

void TabStripLayoutHelper::UpdateCachedTabWidth(int tab_index,
                                                int tab_width,
                                                bool active) {
  if (active)
    active_tab_width_ = tab_width;
  else
    inactive_tab_width_ = tab_width;
}

bool TabStripLayoutHelper::WidthsConstrainedForClosingMode() {
  return tab_width_override_.has_value() ||
         tabstrip_width_override_.has_value();
}
