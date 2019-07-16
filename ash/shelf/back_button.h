// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_BACK_BUTTON_H_
#define ASH_SHELF_BACK_BUTTON_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/shelf/shelf_control_button.h"
#include "base/macros.h"

namespace ash {

class ShelfButtonDelegate;

// The back button shown on the shelf when tablet mode is enabled. Its opacity
// and visiblity are handled by its parent, ShelfView, to ensure the fade
// in/out of the icon matches the movement of ShelfView's items.
class ASH_EXPORT BackButton : public ShelfControlButton {
 public:
  static const char kViewClassName[];

  BackButton(Shelf* shelf, ShelfButtonDelegate* shelf_button_delegate);
  ~BackButton() override;

  // views::Button:
  void NotifyClick(const ui::Event& event) override;
  void PaintButtonContents(gfx::Canvas* canvas) override;
  const char* GetClassName() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BackButton);
};

}  // namespace ash

#endif  // ASH_SHELF_BACK_BUTTON_H_
