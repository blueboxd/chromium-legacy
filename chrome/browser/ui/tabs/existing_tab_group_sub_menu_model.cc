// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/existing_tab_group_sub_menu_model.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/native_theme/native_theme.h"

namespace {

// The tab group icon is a circle that reflects the color of the group in the
// tab strip. Because it's a simple, colored shape, we use a CanvasImageSource
// to draw the icon directly into the menu, rather than passing in a vector.
class TabGroupIconImageSource : public gfx::CanvasImageSource {
 public:
  // Note: This currently matches the size of the empty tab group header, but
  // it doesn't share the same variable because this icon should remain
  // constrained to a menu icon size.
  static constexpr int kIconSize = 14;

  explicit TabGroupIconImageSource(
      const tab_groups::TabGroupVisualData* visual_data);
  ~TabGroupIconImageSource() override;

 private:
  SkColor GetColor();

  // gfx::CanvasImageSource overrides:
  void Draw(gfx::Canvas* canvas) override;

  ui::NativeTheme* native_theme_;
  const tab_groups::TabGroupVisualData* visual_data_;

  DISALLOW_COPY_AND_ASSIGN(TabGroupIconImageSource);
};

TabGroupIconImageSource::TabGroupIconImageSource(
    const tab_groups::TabGroupVisualData* visual_data)
    : CanvasImageSource(gfx::Size(kIconSize, kIconSize)),
      native_theme_(ui::NativeTheme::GetInstanceForNativeUi()),
      visual_data_(visual_data) {}

TabGroupIconImageSource::~TabGroupIconImageSource() = default;

SkColor TabGroupIconImageSource::GetColor() {
  const tab_groups::TabGroupColor color_data =
      tab_groups::GetTabGroupColorSet().at(visual_data_->color());
  return native_theme_->ShouldUseDarkColors() ? color_data.dark_theme_color
                                              : color_data.light_theme_color;
}

void TabGroupIconImageSource::Draw(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);
  flags.setColor(GetColor());
  canvas->DrawCircle(gfx::PointF(kIconSize / 2, kIconSize / 2), kIconSize / 2,
                     flags);
}

}  // namespace

constexpr int kFirstCommandIndex =
    TabStripModel::ContextMenuCommand::CommandLast + 1;

ExistingTabGroupSubMenuModel::ExistingTabGroupSubMenuModel(TabStripModel* model,
                                                           int context_index)
    : SimpleMenuModel(this) {
  model_ = model;
  context_index_ = context_index;
  Build();
}

void ExistingTabGroupSubMenuModel::Build() {
  // Start command ids after the parent menu's ids to avoid collisions.
  int group_index = kFirstCommandIndex;
  for (tab_groups::TabGroupId group : GetOrderedTabGroups()) {
    if (ShouldShowGroup(model_, context_index_, group)) {
      const TabGroup* tab_group = model_->group_model()->GetTabGroup(group);
      AddItemWithIcon(
          group_index, tab_group->GetDisplayedTitle(),
          gfx::ImageSkia(std::make_unique<TabGroupIconImageSource>(
                             tab_group->visual_data()),
                         gfx::Size(TabGroupIconImageSource::kIconSize,
                                   TabGroupIconImageSource::kIconSize)));
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
