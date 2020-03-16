// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_FLOATING_MENU_BUTTON_H_
#define ASH_SYSTEM_ACCESSIBILITY_FLOATING_MENU_BUTTON_H_

#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/top_shortcut_button.h"

namespace views {
class InkDropMask;
}

namespace gfx {
struct VectorIcon;
class Size;
}  // namespace gfx

namespace ash {

// Button view that is used in floating menu.

class FloatingMenuButton : public TopShortcutButton {
 public:
  FloatingMenuButton(views::ButtonListener* listener,
                     const gfx::VectorIcon& icon,
                     int accessible_name_id,
                     bool flip_for_rtl,
                     int size = kTrayItemSize,
                     bool draw_highlight = true);

  ~FloatingMenuButton() override;

  // views::Button:
  const char* GetClassName() const override;

  // Set the vector icon shown in a circle.
  void SetVectorIcon(const gfx::VectorIcon& icon);

  // Change the toggle state.
  void SetToggled(bool toggled);

  bool IsToggled() { return toggled_; }

  // TopShortcutButton:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  gfx::Size CalculatePreferredSize() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // Used in tests.
  void SetId(int id);

 private:
  void UpdateImage();

  const gfx::VectorIcon* icon_;
  // True if the button is currently toggled.
  bool toggled_ = false;
  int size_;
  const bool draw_highlight_;

  DISALLOW_COPY_AND_ASSIGN(FloatingMenuButton);
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_FLOATING_MENU_BUTTON_H_
