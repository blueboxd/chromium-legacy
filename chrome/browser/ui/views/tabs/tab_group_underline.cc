// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_underline.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/tabs/tab_group_visual_data.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/view.h"

constexpr int TabGroupUnderline::kStrokeThickness;

TabGroupUnderline::TabGroupUnderline(TabStrip* tab_strip, TabGroupId group)
    : tab_strip_(tab_strip), group_(group) {
  UpdateVisuals();
}

void TabGroupUnderline::OnPaint(gfx::Canvas* canvas) {
  UpdateVisuals();
  OnPaintBackground(canvas);
}

void TabGroupUnderline::UpdateVisuals() {
  SkColor color = GetColor();

  const int start_x = GetStart();
  const int end_x = GetEnd();

  const int start_y = tab_strip_->bounds().height() - 1;

  SetBounds(start_x, start_y - kStrokeThickness, end_x - start_x,
            kStrokeThickness);
  SetBackground(views::CreateSolidBackground(color));
}

SkColor TabGroupUnderline::GetColor() const {
  const TabGroupVisualData* data =
      tab_strip_->controller()->GetVisualDataForGroup(group_);

  return data->color();
}

int TabGroupUnderline::GetStart() const {
  const gfx::Rect group_header_bounds =
      tab_strip_->group_header(group_)->bounds();

  constexpr int kInset = 20;
  return group_header_bounds.x() + kInset;
}

int TabGroupUnderline::GetEnd() const {
  const std::vector<int> tabs_in_group =
      tab_strip_->controller()->ListTabsInGroup(group_);
  const int last_tab_index = tabs_in_group[tabs_in_group.size() - 1];
  const gfx::Rect& last_tab_bounds =
      tab_strip_->tab_at(last_tab_index)->bounds();

  constexpr int kInset = 20;
  return last_tab_bounds.right() - kInset;
}
