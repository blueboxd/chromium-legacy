// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_BUTTON_H_
#define ASH_SHELF_SHELF_BUTTON_H_

#include "ash/ash_export.h"
#include "ui/views/controls/button/button.h"

namespace ash {
class InkDropButtonListener;
class ShelfView;

// Button used for items on the shelf.
class ASH_EXPORT ShelfButton : public views::Button {
 public:
  explicit ShelfButton(ShelfView* shelf_view);
  ~ShelfButton() override;

  // views::Button
  const char* GetClassName() const override;
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void NotifyClick(const ui::Event& event) override;
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;

 protected:
  ShelfView* shelf_view() { return shelf_view_; }

 private:
  // The shelf view hosting this button.
  ShelfView* shelf_view_;

  InkDropButtonListener* listener_;

  DISALLOW_COPY_AND_ASSIGN(ShelfButton);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_BUTTON_H_
