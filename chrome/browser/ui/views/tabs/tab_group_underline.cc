// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_underline.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_group_visual_data.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_group_views.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/view.h"

constexpr int TabGroupUnderline::kStrokeThickness;

TabGroupUnderline::TabGroupUnderline(TabGroupViews* tab_group_views,
                                     TabGroupId group)
    : tab_group_views_(tab_group_views), group_(group) {
  // Set non-zero bounds to start with, so that painting isn't pruned.
  // Needed because UpdateBounds() happens during OnPaint(), which is called
  // after painting is pruned.
  const int y = GetLayoutConstant(TAB_HEIGHT) - 1;
  SetBounds(0, y - kStrokeThickness, kStrokeThickness * 2, kStrokeThickness);
}

void TabGroupUnderline::OnPaint(gfx::Canvas* canvas) {
  UpdateBounds();

  SkPath path = GetPath();
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(tab_group_views_->GetGroupColor());
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawPath(path, flags);
}

void TabGroupUnderline::UpdateBounds() {
  const int start_x = GetStart();
  const int end_x = GetEnd();

  // The width may be zero if the group underline and header are initialized at
  // the same time, as with tab restore. In this case, don't update the bounds
  // and defer to the next paint cycle.
  if (end_x <= start_x)
    return;

  const int y = tab_group_views_->GetBounds().height() - 1;
  SetBounds(start_x, y - kStrokeThickness, end_x - start_x, kStrokeThickness);
}

// static
int TabGroupUnderline::GetStrokeInset() {
  return TabStyle::GetTabOverlap() + kStrokeThickness;
}

int TabGroupUnderline::GetStart() const {
  const gfx::Rect group_bounds = tab_group_views_->GetBounds();

  return group_bounds.x() + GetStrokeInset();
}

int TabGroupUnderline::GetEnd() const {
  const gfx::Rect group_bounds = tab_group_views_->GetBounds();

  const Tab* last_grouped_tab = tab_group_views_->GetLastTabInGroup();
  if (!last_grouped_tab)
    return group_bounds.right() - GetStrokeInset();

  return group_bounds.right() +
         (last_grouped_tab->IsActive() ? kStrokeThickness : -GetStrokeInset());
}

SkPath TabGroupUnderline::GetPath() const {
  SkPath path;

  path.moveTo(0, kStrokeThickness);
  path.arcTo(kStrokeThickness, kStrokeThickness, 0, SkPath::kSmall_ArcSize,
             SkPathDirection::kCW, kStrokeThickness, 0);
  path.lineTo(width() - kStrokeThickness, 0);
  path.arcTo(kStrokeThickness, kStrokeThickness, 0, SkPath::kSmall_ArcSize,
             SkPathDirection::kCW, width(), kStrokeThickness);
  path.close();

  return path;
}
