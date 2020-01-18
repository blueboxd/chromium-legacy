// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WM_HIGHLIGHT_ITEM_BORDER_H_
#define ASH_WM_WM_HIGHLIGHT_ITEM_BORDER_H_

#include "base/macros.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/border.h"

namespace ash {

// Defines a border to be used on the views of window management items which can
// be highlighted in overview or window cycle, such as the DeskPreviewView,
// NewDeskButton and WindowMiniView. This paints a border around the view with
// an empty gap (padding) in-between, so that the border is more obvious against
// white or light backgrounds. If a |SK_ColorTRANSPARENT| was provided, it will
// paint nothing.
class WmHighlightItemBorder : public views::Border {
 public:
  explicit WmHighlightItemBorder(int corner_radius);
  ~WmHighlightItemBorder() override = default;

  void set_extra_margin(int extra_margin) { extra_margin_ = extra_margin; }

  // views::Border:
  void Paint(const views::View& view, gfx::Canvas* canvas) override;
  gfx::Insets GetInsets() const override;
  gfx::Size GetMinimumSize() const override;

 private:
  const int corner_radius_;

  // Some views which use this border have padding for other UI purposes (ie.
  // they paint their contents insetted from |View::GetLocalBounds()|. Such
  // views are responsible for letting this class know how much padding is used
  // so adjustments can be made accordingly.
  // TODO(sammiequon): Change OverviewItemView to not have its own margin, then
  // this won't be needed.
  int extra_margin_ = 0;

  DISALLOW_COPY_AND_ASSIGN(WmHighlightItemBorder);
};

}  // namespace ash

#endif  // ASH_WM_WM_HIGHLIGHT_ITEM_BORDER_H_
