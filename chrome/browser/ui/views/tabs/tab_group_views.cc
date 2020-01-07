// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_views.h"

#include <utility>

#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_group_highlight.h"
#include "chrome/browser/ui/views/tabs/tab_group_underline.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_types.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

TabGroupViews::TabGroupViews(TabStrip* tab_strip,
                             const tab_groups::TabGroupId& group)
    : tab_strip_(tab_strip), group_(group) {
  header_ = std::make_unique<TabGroupHeader>(tab_strip_, group_);
  header_->set_owned_by_client();

  underline_ = std::make_unique<TabGroupUnderline>(this, group_);
  underline_->set_owned_by_client();

  highlight_ = std::make_unique<TabGroupHighlight>(this, group_);
  highlight_->set_owned_by_client();
}

TabGroupViews::~TabGroupViews() {}

void TabGroupViews::UpdateVisuals() {
  header_->VisualsChanged();
  underline_->SchedulePaint();
  highlight_->SchedulePaint();

  const int active_index = tab_strip_->controller()->GetActiveIndex();
  if (active_index != TabStripModel::kNoTab)
    tab_strip_->tab_at(active_index)->SchedulePaint();
}

gfx::Rect TabGroupViews::GetBounds() const {
  const Tab* last_tab = GetLastTabInGroup();
  if (!last_tab)
    return header_->bounds();

  const int start_x = header_->x();
  const int end_x = last_tab->bounds().right();

  if (end_x <= start_x)
    return header_->bounds();

  const int y = header_->y();
  const int height = header_->height();

  return gfx::Rect(start_x, y, end_x - start_x, height);
}

Tab* TabGroupViews::GetLastTabInGroup() const {
  const std::vector<int> tabs_in_group =
      tab_strip_->controller()->ListTabsInGroup(group_);

  if (tabs_in_group.size() <= 0)
    return nullptr;

  const int last_tab_index = tabs_in_group[tabs_in_group.size() - 1];
  return tab_strip_->tab_at(last_tab_index);
}

SkColor TabGroupViews::GetGroupColor() const {
  const tab_groups::TabGroupColorId color_id =
      tab_strip_->controller()->GetGroupColorId(group_);
  return tab_strip_->GetPaintedGroupColor(color_id);
}

SkColor TabGroupViews::GetTabBackgroundColor() const {
  return tab_strip_->GetTabBackgroundColor(
      TabActive::kInactive, BrowserFrameActiveState::kUseCurrent);
}

SkColor TabGroupViews::GetGroupBackgroundColor() const {
  const SkColor active_color = tab_strip_->GetTabBackgroundColor(
      TabActive::kActive, BrowserFrameActiveState::kUseCurrent);
  return SkColorSetA(active_color, gfx::Tween::IntValueBetween(
                                       TabStyle::kSelectedTabOpacity,
                                       SK_AlphaTRANSPARENT, SK_AlphaOPAQUE));
}
