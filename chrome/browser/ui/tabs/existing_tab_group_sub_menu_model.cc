// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/existing_tab_group_sub_menu_model.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"

constexpr int kFirstCommandIndex =
    TabStripModel::ContextMenuCommand::CommandLast + 1;

ExistingTabGroupSubMenuModel::ExistingTabGroupSubMenuModel(
    ui::SimpleMenuModel::Delegate* parent_delegate,
    TabStripModel* model,
    int context_index)
    : SimpleMenuModel(this),
      parent_delegate_(parent_delegate),
      model_(model),
      context_index_(context_index) {
  Build();
}

void ExistingTabGroupSubMenuModel::Build() {
  // Start command ids after the parent menu's ids to avoid collisions.
  int group_index = kFirstCommandIndex;
  const auto& tp = ThemeService::GetThemeProviderForProfile(model_->profile());
  AddItemWithStringId(TabStripModel::CommandAddToNewGroup,
                      IDS_TAB_CXMENU_SUBMENU_NEW_GROUP);
  AddSeparator(ui::NORMAL_SEPARATOR);
  for (tab_groups::TabGroupId group : GetOrderedTabGroups()) {
    if (ShouldShowGroup(model_, context_index_, group)) {
      const TabGroup* tab_group = model_->group_model()->GetTabGroup(group);
      const base::string16 group_title = tab_group->visual_data()->title();
      const base::string16 displayed_title =
          group_title.empty() ? tab_group->GetContentString() : group_title;
      constexpr int kIconSize = 14;
      const int color_id =
          GetTabGroupContextMenuColorId(tab_group->visual_data()->color());
      // TODO (kylixrd): Investigate passing in color_id in order to color the
      // icon using the ColorProvider.
      AddItemWithIcon(group_index, displayed_title,
                      ui::ImageModel::FromVectorIcon(
                          kTabGroupIcon, tp.GetColor(color_id), kIconSize));
    }
    group_index++;
  }
}

std::vector<tab_groups::TabGroupId>
ExistingTabGroupSubMenuModel::GetOrderedTabGroups() {
  std::vector<tab_groups::TabGroupId> ordered_groups;
  base::Optional<tab_groups::TabGroupId> current_group = base::nullopt;
  for (int i = 0; i < model_->count(); ++i) {
    base::Optional<tab_groups::TabGroupId> new_group =
        model_->GetTabGroupForTab(i);
    if (new_group.has_value() && new_group != current_group)
      ordered_groups.push_back(new_group.value());
    current_group = new_group;
  }
  return ordered_groups;
}

bool ExistingTabGroupSubMenuModel::IsCommandIdChecked(int command_id) const {
  return false;
}

bool ExistingTabGroupSubMenuModel::IsCommandIdEnabled(int command_id) const {
  return true;
}

void ExistingTabGroupSubMenuModel::ExecuteCommand(int command_id,
                                                  int event_flags) {
  if (command_id == TabStripModel::CommandAddToNewGroup) {
    parent_delegate_->ExecuteCommand(TabStripModel::CommandAddToNewGroup,
                                     event_flags);
    return;
  }
  const int group_index = command_id - kFirstCommandIndex;
  DCHECK_LT(size_t{group_index}, model_->group_model()->ListTabGroups().size());
  base::RecordAction(base::UserMetricsAction("TabContextMenu_NewTabInGroup"));
  model_->ExecuteAddToExistingGroupCommand(context_index_,
                                           GetOrderedTabGroups()[group_index]);
}

// static
bool ExistingTabGroupSubMenuModel::ShouldShowSubmenu(TabStripModel* model,
                                                     int context_index) {
  for (tab_groups::TabGroupId group : model->group_model()->ListTabGroups()) {
    if (ShouldShowGroup(model, context_index, group)) {
      return true;
    }
  }
  return false;
}

// static
bool ExistingTabGroupSubMenuModel::ShouldShowGroup(
    TabStripModel* model,
    int context_index,
    tab_groups::TabGroupId group) {
  if (!model->IsTabSelected(context_index)) {
    if (group != model->GetTabGroupForTab(context_index))
      return true;
  } else {
    for (int index : model->selection_model().selected_indices()) {
      if (group != model->GetTabGroupForTab(index)) {
        return true;
      }
    }
  }
  return false;
}
