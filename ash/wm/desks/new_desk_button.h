// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_NEW_DESK_BUTTON_H_
#define ASH_WM_DESKS_NEW_DESK_BUTTON_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "base/macros.h"
#include "ui/views/controls/button/label_button.h"

namespace ash {

// A button view that shows up in the top-right corner of the screen when
// overview mode is on, which is used to create a new virtual desk.
class ASH_EXPORT NewDeskButton
    : public views::LabelButton,
      public OverviewHighlightController::OverviewHighlightableView {
 public:
  explicit NewDeskButton(views::ButtonListener* listener);
  ~NewDeskButton() override = default;

  // Update the button's enable/disable state based on current desks state.
  void UpdateButtonState();

  void OnButtonPressed();

  // LabelButton:
  const char* GetClassName() const override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  SkColor GetInkDropBaseColor() const override;
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override;
  std::unique_ptr<views::LabelButtonBorder> CreateDefaultBorder()
      const override;

  // OverviewHighlightController::OverviewHighlightableView:
  views::View* GetView() override;
  gfx::Rect GetHighlightBoundsInScreen() override;
  gfx::RoundedCornersF GetRoundedCornersRadii() const override;
  void MaybeActivateHighlightedView() override;
  void MaybeCloseHighlightedView() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NewDeskButton);
};

}  // namespace ash

#endif  // ASH_WM_DESKS_NEW_DESK_BUTTON_H_
