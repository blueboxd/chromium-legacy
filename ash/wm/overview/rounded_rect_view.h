// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_ROUNDED_RECT_VIEW_H_
#define ASH_WM_OVERVIEW_ROUNDED_RECT_VIEW_H_

#include "ui/views/view.h"

namespace gfx {
class Canvas;
}  // namespace gfx

namespace ash {

// A View having rounded corners and a specified background color which is
// only painted within the bounds defined by the rounded corners.
class RoundedRectView : public views::View {
 public:
  RoundedRectView(int corner_radius, SkColor background_color);
  RoundedRectView(const RoundedRectView&) = delete;
  RoundedRectView& operator=(const RoundedRectView&) = delete;
  ~RoundedRectView() override;

  void SetCornerRadius(int radius);

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  int corner_radius_;
  const SkColor background_color_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_ROUNDED_RECT_VIEW_H_
